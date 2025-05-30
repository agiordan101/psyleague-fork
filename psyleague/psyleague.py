#!/usr/bin/env python3

# Author: Psyho
# Twitter: twitter.com/fakepsyho
# BlueSky: psyho.bsky.social

#TODO:
# HIGH PRIORITY
# -add ability to show ratings for subset of games on the main scoreboard
# -show: add --persistent X mode to constantly refresh results and X is "cooldown" between refreshes
# -run: add option to show the ranking periodically?
# -db should contain info about the rating model used?
# -change psyleague.db format to json?

# -add a bot having only an executable? (allows for bots without source code / in a different language)
# -update readme
# -peek at the next message in order to do a single ranking recalculation?
# -add option for updating model source (essentially BOT_REMOVE+BOT_ADD)?
# -auto reduce number of bots (greedily remove a bot and see how it affects the overall ranking (minimize MSE?))
# -add error checking to CG play_game.py (file not existing & problem with parsing JSON)

# LOW PRIORITY
# -choose_match: update matchmaking (more priority to top bots)
# -worker error shouldn't immediately interrupt main thread (small chance for corrupting results)
# -wrapper for \r printing

# ???
# -rename *.db to *.bots?
# -switch to JSON for msg?
# -add an option to update default config? (psyleague config -> psyleague config new)
# -add option to use a different config? 

__version__ = '0.4.1'

import re
import signal
import time
import shutil
import random
import json
import argparse
import sys
import os.path
import subprocess
import queue
import traceback
import csv
import numpy as np
from datetime import datetime
from threading import Thread
from typing import List, Dict, Tuple, Any, Union

import tabulate
import colorama
import portalocker
import trueskill as ts
from openskill.models import *
import choix
import toml
from tqdm import tqdm

CONFIG_FILE = 'psyleague.cfg'

args = None
cfg = None

games_queue = queue.Queue()
results_queue = queue.Queue()

def try_str_to_numeric(x):
    if x is None:
        return None
    try: 
        return int(x)
    except ValueError:
        try:
            return float(x)
        except ValueError:
            return x


class RollingStat:
    def __init__(self, window_length: int):
        self.window_length = window_length
        self.data = []
        
    def add(self) -> None:
        self.data.append(time.time())
        
    def get_count(self) -> int:
        t = time.time()
        while len(self.data) and self.data[0] + self.window_length < t:
            del self.data[0]
        return len(self.data)
    

#region Container classes

class Bot:
    def __init__(self, name, description, mu=25.0, sigma=25/3, games=0, errors=0, active=1, cdate=None):
        self.name = name
        self.description = description
        self.mu = mu
        self.sigma = sigma
        self.games = games
        self.errors = errors
        self.active = active
        self.cdate = cdate
        
    @classmethod
    def from_str(cls, input):
        a = [s.strip() for s in input.split(':')]
        return cls(a[0], a[1], float(a[2]), float(a[3]), int(a[4]), int(a[5]), int(a[6]), datetime.strptime(a[7], '%Y-%m-%d %H-%M-%S') if a[7] != 'n/a' else None)
        
    def __repr__(self):
        return f'{self.name} : {self.description} : {self.mu} : {self.sigma} : {self.games} : {self.errors} : {self.active} : {self.cdate.strftime("%Y-%m-%d %H-%M-%S") if self.cdate else "n/a"}'
        
    def to_ts(self) -> ts.Rating:
        return ts.Rating(mu=self.mu, sigma=self.sigma)
    
    def to_os(self) -> PlackettLuceRating:
        return PlackettLuceRating(mu=self.mu, sigma=self.sigma)
        
    def update(self, data) -> None:
        if isinstance(data, ts.Rating):
            self.mu = data.mu
            self.sigma = data.sigma
        elif isinstance(data, PlackettLuceRating):
            self.mu = data.mu
            self.sigma = data.sigma
        elif isinstance(data, float):
            self.mu = data
            self.sigma = 0
        else:
            assert False, 'Unknown rating type'

    
class Game:
    def __init__(self, players=None, ranks=None, errors=None, *, str=None):
        self.players = players
        self.ranks = ranks
        self.errors = errors
        self.test_data = {}
        self.player_data = [{} for _ in range(cfg['n_players'])]
        if str:
            try:
                data = json.loads(str)
            except json.JSONDecodeError:
                print(f'[Error]: Game JSON is invalid: {str}')
                sys.exit(1)
            self.players = data['players']
            self.ranks = data['ranks']
            self.errors = data['errors']
            self.test_data = {k: try_str_to_numeric(v) for k, v in data['test_data'].items()}
            self.player_data = [{k: try_str_to_numeric(v) for k, v in d.items()} for d in data['player_data']]
            
    def __repr__(self):
        return json.dumps(self.__dict__)
        
#endregion    

#region "DB" functions

def load_config(path: str) -> Dict[str, Any]:
    with open(path, 'r') as f:
        return toml.load(f)
    
lock_args = {'timeout': 2.0, 'check_interval': 0.02}

lock_file = None
def unique_run() -> bool:
    try:
        global lock_file
        lock_file = open(cfg['file_lock'], 'a')
        portalocker.lock(lock_file, portalocker.LOCK_EX | portalocker.LOCK_NB)
        return True
    except portalocker.LockException:
        return False
    
def log(*args) -> None:
    with portalocker.Lock(cfg['file_log'], 'a', **lock_args) as f:
        dt = datetime.now().replace(microsecond=0)
        f.write('[' + str(dt) + '] ' + ' '.join(map(str, args)) + '\n')

def send_msg(msg: str) -> None:
    with portalocker.Lock(cfg['file_msg'], 'a', **lock_args) as f:
        f.write(msg + '\n')
    
def receive_msgs() -> List[str]:
    if not os.path.exists(cfg['file_msg']):
        return []
    with portalocker.Lock(cfg['file_msg'], 'r+', **lock_args) as f:
        msgs = f.readlines()
        f.truncate(0)
        return [s.strip() for s in msgs]
        
def add_game(game: Game) -> None:
    with portalocker.Lock(cfg['file_games'], 'a', **lock_args) as f:
        f.write(str(game) + '\n')
        
def load_all_games() -> List[Game]:
    if not os.path.exists(cfg['file_games']):
        return []
    with portalocker.Lock(cfg['file_games'], 'r', **lock_args) as f:
        data = f.readlines()
    return [Game(str=s.strip()) for s in data]

def save_all_games(games: List[Game]) -> None:
    with portalocker.Lock(cfg['file_games'], 'w', **lock_args) as f:
        for g in games:
            f.write(str(g) + '\n')

# TODO: write to string first and then output with a single call?
saving_db = False

def save_db(bots: Dict[str, Bot]) -> None:
    global saving_db
    saving_db = True
    with portalocker.Lock(cfg['file_db'], 'w', **lock_args) as f:
        for b in bots.values():
            f.write(f'{str(b)}\n')
    saving_db = False

def load_db() -> Dict[str, Bot]:
    if not os.path.exists(cfg['file_db']):
        return {}
    with portalocker.Lock(cfg['file_db'], 'r', **lock_args) as f:
        data = f.readlines()
    bots = [Bot.from_str(line.strip()) for line in data]
    return {b.name: b for b in bots}    

#endregion

#region helper functions
    
def update_ranking(bots: Dict[str, Bot], games: Union[List[Game], Game], whole_ranking=False, progress_bar=False, model=None) -> None:
    model = model or cfg['model']

    if model == 'trueskill':
        ts.setup(tau=cfg['model_tau'], draw_probability=cfg['model_draw_prob'])
    elif model == 'openskill':
        os_model = PlackettLuce(tau=cfg['model_tau'], balance=False)
    elif model == 'global':
        pass # nothing to initialize
    else:
        assert False, f'Invalid rating model: {cfg["model"]}'

    if not isinstance(games, list):
        games = [games]

    if model == 'global':
        if not whole_ranking: 
            return
        
        for game in games:
            for player in game.players:
                bots[player].games += 1

        bots_names = list(bots.keys())
        bots_ids = {p: i for i, p in enumerate(bots.keys())}
        games_ids = [([bots_ids[p] for p in game.players], game.ranks) for game in games]

        if cfg['n_players'] == 2:
            results_count = [[0] * len(bots) for _ in range(len(bots))]
            for ids, ranks in games_ids:
                if ranks[0] < ranks[1]:
                    results_count[ids[0]][ids[1]] += 2
                elif ranks[0] > ranks[1]:
                    results_count[ids[1]][ids[0]] += 2
                else:
                    results_count[ids[0]][ids[1]] += 1
                    results_count[ids[1]][ids[0]] += 1
            ratings = choix.ilsr_pairwise_dense(np.array(results_count), alpha=cfg['model_alpha'])
        else:
            cgames = []
            for ids, ranks in games_ids:
                used = [False] * cfg['n_players']
                cgame = []
                for _ in range(cfg['n_players']):
                    best = None
                    count = 0
                    for i in range(cfg['n_players']):
                        if used[i]: 
                            continue
                        if best is None or ranks[i] < ranks[best]:
                            best = i
                            count = 1
                            continue
                        if ranks[i] == ranks[best]:
                            count += 1
                            if random.randrange(count) == 0:
                                best = i
                    used[best] = True
                    cgame.append(ids[best])
                cgames.append(cgame)
            ratings = choix.ilsr_rankings(len(bots), cgames, alpha=cfg['model_alpha'])

        for i, player in enumerate(bots_names):
            bots[player].update((ratings[i] + 10) * cfg['model_scale'])
        return

    if progress_bar:
        games = tqdm(games)

    for game in games:
        for i, player in enumerate(game.players):
            bots[player].games += 1
            bots[player].errors += game.errors[i]

        if cfg['skip_errors'] and any(game.errors):
            continue

        if model == 'trueskill':
            ratings = ts.rate([(bots[player].to_ts(), ) for player in game.players], ranks=game.ranks)
            for i, player in enumerate(game.players):
                bots[player].update(ratings[i][0])
        elif model == 'openskill':
            ratings = os_model.rate([[bots[player].to_os()] for player in game.players], ranks=game.ranks)
            for i, player in enumerate(game.players):
                bots[player].update(ratings[i][0])
        else:
            assert False, f'Invalid rating model: {cfg["model"]}'

            


def recalculate_ranking(bots: Dict[str, Bot], games: List[Game], model: str = None) -> Dict[str, Bot]:
    new_bots = {b.name: Bot(name=b.name, description=b.description, active=b.active, cdate=b.cdate) for b in bots.values()}
    for game in games:
        for player in game.players:
            if player not in new_bots:
                new_bots[player] = Bot(name=player, description='n/a')
    update_ranking(new_bots, games, whole_ranking=True, progress_bar=cfg['show_progress'], model=model)
    return new_bots


def play_games(bots: List[str], verbose: bool=False) -> Union[Game, List[Game]]:
    # TODO: add error handling?
    cmd = cfg['cmd_play_game']
    if '%ALL_PLAYERS%' in cmd:
        words = cmd.split()
        for i, word in enumerate(words):
            if '%ALL_PLAYERS%' in word:
                words[i] = ' '.join([words[i].replace('%ALL_PLAYERS%', f'%P{j+1}%') for j in range(cfg['n_players'])])
        cmd = ' '.join(words)
        
    cmd = cmd.replace('%DIR%', cfg['dir_bots'])
    for i in range(4):
        tag = f'%P{i+1}%'
        if tag in cmd:
            cmd = cmd.replace(tag, bots[i])
    
    if verbose:
        print(f'Playing Game: {cmd}')
    
    if os.name == 'nt': # windows
        task = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
    else: 
        task = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, preexec_fn=lambda: signal.signal(signal.SIGINT, signal.SIG_IGN))
            
    if task.returncode:
        print(f'Fatal Error: Play Game command {cmd} returned with return code {task.returncode}')
        os._exit(1)
        
    output = task.stdout.decode('UTF-8').strip()
    if verbose:
        print(f'{cmd} produced output: {output}')
    
    try:
        data = json.loads(output)
    except json.JSONDecodeError:
        print(f'[Error] Play Game command {cmd} produced invalid JSON: {output}')
        sys.exit(1)

    if (isinstance(data, dict)):
        data['players'] = bots
        return Game(str=json.dumps(data))
    else:
        for d in data:
            d['players'] = bots
        return [Game(str=json.dumps(d)) for d in data]


def choose_match(bots: Dict[str, Bot]) -> List[str]:
    l_bots = [b for b in bots.values() if b.active]

    if len(l_bots) < cfg['n_players']: 
        return None    
        
    # find first player
    min_bots = [b for b in l_bots if b.games < cfg['mm_min_matches']]
    selected_bot = None
    if len(min_bots) and random.random() < cfg['mm_min_matches_preference']:
        p1 = random.choice(min_bots)
    else:
        p1 = random.choice(l_bots)
        
    # find remaining players
    while True:
        players = [p1.name]
        for _ in range(cfg['n_players'] - 1):
            while True:
                p = random.choice(l_bots).name
                if p not in players:
                    players.append(p)
                    break   
        if any([p != p1 for p in players]):
            break
   
    random.shuffle(players)
    return players

#endregion

#region Mode functions

def mode_config() -> None:
    source_path = os.path.join(os.path.dirname(__file__), 'psyleague.cfg')
    target_path = os.path.join(os.getcwd(), 'psyleague.cfg')
    if os.path.exists(target_path):
        print('Config file already exist')
        return
    print(f'Creating new config file at psyleague.cfg')
    shutil.copy(source_path, target_path)


def mode_run() -> None:
    log('[Action] Run')

    if not unique_run():
        print('[Error] Another instance of Psyleague server is already running')
        sys.exit(1)
    
    if not args.silent:
        print('Starting Psyleague server, press Ctrl+C to kill it')
    
    bots = load_db()
    games = load_all_games()
    
    games_played = 0
    games_total = args.games or sys.maxsize
    games_left = games_total
    games_stat = RollingStat(60.0)
    last_msg_time = time.time()
    
    try:
        start_time = time.time()
        
        def worker_loop() -> None:
            while True:
                try:
                    players = games_queue.get(block=False)
                    if players is None: 
                        break
                    games = play_games(players, args.verbose)
                    if isinstance(games, Game):
                        games = [games]
                    for game in games:
                        results_queue.put(game)
                except queue.Empty:
                    time.sleep(0.1)
                except KeyboardInterrupt:
                    print('Worker Interrupted, this should never happen!')
                    break
                except:
                    print('Fatal error in one of the workers')
                    traceback.print_exc()
                    os._exit(1)
                    
                    
        workers = [Thread(target=worker_loop) for _ in range(args.workers if args.workers is not None else cfg['n_workers'])]
        for worker in workers:
            worker.start()
            
        while True:
            time.sleep(0.1)
            
            # retrieve and process all messages
            for msg in receive_msgs():
                if not args.silent:
                    print(f'{msg}' + ' '*(70-len(msg))) # XXX: Ugly, wrapper for this would be better
                a = [s.strip() for s in msg.split(':')]
                msg_type = a[0]
                if msg_type == 'ADD_BOT':
                    name = a[1]
                    description = a[2]
                    # TODO: add some error checking
                    bots[name] = Bot(name, description, cdate=datetime.now())
                    save_db(bots)
                elif msg_type == 'UPDATE_BOT':
                    name = a[1]
                    new_name = a[2]
                    description = a[3]
                    bots[name].description = description
                    if new_name != name:
                        bots[new_name] = bots[name]
                        bots[new_name].name = new_name
                        del bots[name]
                        games = load_all_games()
                        for g in games:
                            g.players = [s if s != name else new_name for s in g.players]
                        save_all_games(games)
                    save_db(bots)
                elif msg_type == 'STOP_BOT':
                    name = a[1]
                    # TODO: add some error checking
                    bots[name].active = 0
                    save_db(bots)
                elif msg_type == 'REMOVE_BOT':
                    games = load_all_games()
                    games_no = len(games)
                    name = a[1]
                    games = [g for g in games if name not in g.players]
                    print(f'Removed {games_no - len(games)} games')
                    del bots[name]
                    print('Recalculating ranking...')
                    bots = recalculate_ranking(bots, games)
                    save_all_games(games)
                    save_db(bots)
                else:
                    assert False, f'Unknown message type: {msg_type}'
                
            # add new games to the queue
            while games_queue.qsize() < cfg['n_workers'] * 2 and games_left > 0:
                players = choose_match(bots)
                if players is None: 
                    break
                games_left -= 1
                games_queue.put(players)
            # TODO: add games_left == 0 handling                
            
            # process results
            try:
                while True:
                    game = results_queue.get(block=False)
                    if args.verbose:
                        print(f'Processing result: {game.players}, outcome: {game.ranks}')
                    if any([player not in bots for player in game.players]):
                        print(f'Warning: Unknown bot in game: {game.players}; skipping game')
                        continue
                    add_game(game)
                    update_ranking(bots, game)
                    save_db(bots) 
                    games_played += 1
                    games_stat.add()
            except queue.Empty:
                pass
                
            if not args.silent and not args.verbose:
                active_bots = sum([1 for b in bots.values() if b.active])
                print(f'\rActive Bots: {active_bots}  Games since launch: {games_played}{f' / {games_total}' if args.games else ''}  Games in the last 60s: {games_stat.get_count()}                    \r', end='')
            
            if games_played >= games_total:
                break
            
    except KeyboardInterrupt:
        print('\nInterrupted by user, waiting for all workers to finish!')
        print('If this doesn\'t happen, press Ctrl+C again to kill all of the workers')
        
    except:
        print('\nFatal Error in the main thread')
        traceback.print_exc()
        os._exit(1)

    if saving_db:
        save_db(bots)        

    # stop running any new games
    try:
        while True:
            games_queue.get(block=False)
    except:
        pass

    for _ in workers:
        games_queue.put(None)

    try:
        while True:
            workers_running = sum(1 for w in workers if w.is_alive())
            print(f'\rNumber of workers still running: {workers_running}                    \r', end='')
            if workers_running == 0:
                break
            time.sleep(.1)
    except:
        os._exit(1)


def mode_bot() -> None:
    if args.cmd == 'add':
        bots = load_db()
        if args.name in bots:
            print(f'Bot {args.name} already exists, ignoring command')
            return
            
        if not args.src:
            args.src = args.name
        
        cmd = args.setup or cfg['cmd_bot_setup']
        cmd = cmd.replace('%DIR%', cfg['dir_bots'])
        cmd = cmd.replace('%NAME%', args.name)
        cmd = cmd.replace('%SRC%', args.src)
        
        os.makedirs(cfg['dir_bots'], exist_ok=True)
        print(f'Running Setup: {cmd}')
        proc = subprocess.run(cmd, shell=True)
        
        # XXX: is this the best way to handle failure?
        if proc.returncode:
            print(f'Setup failed with exit code {proc.returncode}')
            return
        
        if cfg['selfplay_check']:
            print('Running Selfplay Check to verify the bot...')
            players = [args.name] * cfg['n_players']
            games = play_games(players)
            if isinstance(games, Game):
                games = [games]
            if any([any(game.errors) for game in games]):
                print('[Error] Selfplay Check failed, bot will not be added')
                return
        
        log(f'[Action] Add Bot {args.name}')
        send_msg(f'ADD_BOT : {args.name} : {args.description}')
    elif args.cmd == 'update':
        bots = load_db()
        if args.name not in bots:
            print(f'Bot {args.name} doesn\'t exist, ignoring command')
            return
            
        log(f'[Action] Update Bot {args.name}')
        send_msg(f'UPDATE_BOT : {args.name} : {args.new_name or args.name} : {args.description}')
    elif args.cmd == 'stop':
        bots = load_db()
        for bot in args.name.split(','):
            if bot not in bots:
                print(f'Bot {bot} doesn\'t exist, ignoring command')
                return
        for bot in args.name.split(','):
            log(f'[Action] Stop Bot {bot}')
            send_msg(f'STOP_BOT : {bot}')
    elif args.cmd == 'remove':
        bots = load_db()
        for bot in args.name.split(','):
            if bot not in bots:
                print(f'Bot {bot} doesn\'t exist, ignoring command')
                return
        for bot in args.name.split(','):
            log(f'[Action] Remove Bot {bot}')
            send_msg(f'REMOVE_BOT : {bot}')
    else:
        assert False, f'Uknown bot command: {args.cmd}'


def parse_color(color):
    color = color.upper()
    style = colorama.Style.NORMAL
    if '_' in color:
        style, color = color.split('_')
        style = getattr(colorama.Style, style)
    color = getattr(colorama.Fore, color) if color not in ['DEFAULT', '*'] else ''
    return style + color    


def init_colors():
    even_row_cmd = ''
    odd_row_cmd = ''
    header_cmd = ''
    reset_cmd = ''
    if cfg['show_colors']:
        try:
            colorama.init()
            even_row_cmd = parse_color(cfg['even_row_color'])
            odd_row_cmd  = parse_color(cfg['odd_row_color'])
            header_cmd = parse_color(cfg['header_color'])
            reset_cmd = colorama.Style.RESET_ALL
        except AttributeError:
            print(f'[Error] One of the specified colors is not valid. Refer to the cfg file for valid combinations')
    return {'even': even_row_cmd, 'odd': odd_row_cmd, 'header': header_cmd, 'reset': reset_cmd}


class DataTable:
    def __init__(self, headers=None, data=None, format=None, colalign=None):
        self.headers = headers or []
        self.data = data or []
        self.format = format or []
        self.colalign = colalign or []

    def add_column(self, header, data, format, colalign):
        self.headers.append(header)
        self.data.append(data)
        self.format.append(format)
        self.colalign.append(colalign)

    def show(self, output):
        assert output in ['table', 'csv', 'json']
        data = list(zip(*self.data)) #transpose

        if output == 'table':
            if hasattr(tabulate, 'MIN_PADDING'):
                tabulate.MIN_PADDING = 0

            # generate ascii table and output it using colors
            color_cmd = init_colors()
            output = tabulate.tabulate(data, headers=self.headers, floatfmt=self.format, colalign=self.colalign)
            parity = False
            header = True
            for line in output.splitlines():
                if header:
                    if line.startswith('-'):
                        header = False
                    else:
                        line = color_cmd['header'] + line + color_cmd['reset']
                else:
                    line = (color_cmd['even'] if parity else color_cmd['odd']) + line + color_cmd['reset']
                    parity = not parity
                print(line)
        elif output == 'csv':
            writer = csv.writer(sys.stdout)
            writer.writerow(self.headers)
            for line in data:
                writer.writerow([(try_str_to_numeric(f'{line[i]:{self.format[i]}}') if line[i] is not None else '') for i in range(len(self.headers))])
        elif output == 'json':
            for line in data:
                print(json.dumps({self.headers[i]: try_str_to_numeric(f'{line[i]:{self.format[i]}}' if line[i] is not None else '') for i in range(len(self.headers))}))
        

def mode_show() -> None:
    log('[Action] Show')

    bots = load_db()
    games = load_all_games()

    if args.resample or args.filters or args.include or args.exclude or args.model or cfg['model'] == 'global':
        if args.filters:
            available_vars = {var for game in games for var in game.test_data}
            for filter in args.filters:
                var, value = filter.split('=')
                if var not in available_vars:
                    print(f'[Error] There are no games with: {var} available vars: {available_vars}')
                    sys.exit(1)
                games = [game for game in games if var in game.test_data]
                if '-' in value:
                    lo, hi = value.split('-')
                    lo = try_str_to_numeric(lo) if lo else min([game.test_data[var] for game in games])
                    hi = try_str_to_numeric(hi) if hi else max([game.test_data[var] for game in games])
                    games = [game for game in games if lo <= game.test_data[var] <= hi]
                else:
                    value = try_str_to_numeric(value)
                    games = [game for game in games if game.test_data[var] == value]
            if not games:
                print(f'[Error] There are no games matching the filters')
                sys.exit(1)

        if args.include or args.exclude:
            players = set(bots.keys())

            if args.include:
                players = set()
                for pattern in args.include:
                    players.update([b for b in bots if re.match(pattern, b)])
            if args.exclude:
                for pattern in args.exclude:
                    players.difference_update([b for b in bots if re.match(pattern, b)])

            games = [game for game in games if all([p in players for p in game.players])]
            bots = {b: bots[b] for b in players}
                
        if args.resample:
            random.seed(datetime.now())
            games = random.choices(games, k=args.resample)

        print(f'Recalculating ranking using {len(games)} games...')
        bots = recalculate_ranking(bots, games, model=args.model)
        print()

    ranking = sorted(bots.values(), key=lambda b: b.mu-3*b.sigma, reverse=True)

    if args.active:
        ranking = [b for b in ranking if b.active]
        
    if args.best:
        ranking = ranking[:args.best]
        
    original_pos = None
    if args.recent:
        original_pos = {b.name: i+1 for i, b in enumerate(ranking)}
        ranking = sorted(ranking, key=lambda b: b.cdate, reverse=True)
        ranking = ranking[:args.recent]
        ranking = sorted(ranking, key=lambda b: b.mu-3*b.sigma, reverse=True)
        
    
    columns = {}
    columns['pos'] = ('Pos', list(range(1, 1+len(ranking))) if original_pos is None else [f'{i+1} ({original_pos[b.name]})' for i, b in enumerate(ranking)])
    columns['name'] = ('Name', [b.name for b in ranking])
    columns['score'] = ('Score', [b.mu-3*b.sigma for b in ranking])
    columns['games'] = ('Games', [b.games for b in ranking])
    columns['percentage'] = ('%', [f'{min(100, b.games * 100 // cfg['mm_min_matches'])}%' for b in ranking])
    columns['mu'] = ('Mu', [b.mu for b in ranking])
    columns['sigma'] = ('Sigma', [b.sigma for b in ranking])
    columns['errors'] = ('Errors', [b.errors for b in ranking])
    columns['active'] = ('Active', [b.active for b in ranking])
    columns['description'] = ('Description', [b.description for b in ranking])
    try:
        columns['date'] = ('Created', [b.cdate.strftime(cfg['date_format']) if b.cdate else "n/a" for b in ranking])
    except ValueError:
        print(f'[Error] Your date_format: "{cfg["date_format"]}" is invalid')
        sys.exit(1)

    player_vars = {b.name: {} for b in ranking}
    for game in games:
        for player, data in enumerate(game.player_data):
            name = game.players[player]
            if name not in player_vars: 
                continue
            for k, v in data.items():
                player_vars[name][k] = player_vars[name].get(k, 0) + float(v)

    vars = set()
    for name in player_vars:
        vars.update(player_vars[name])

    for var in vars:
        columns[f'pdata:{var}'.lower()] = (var, [player_vars[b.name][var] / b.games if var in player_vars[b.name] else None for b in ranking])

    leaderboard = cfg['leaderboard'].split(',')
    for i, column_name in enumerate(leaderboard):
        if column_name.lower() == 'pdata_all':
            del leaderboard[i]
            for var in sorted(vars, reverse=True):
                leaderboard.insert(i, f'pdata:{var}')
            break
        
    table = DataTable()
    for column_name in leaderboard:
        column_name = column_name.lower()
        optional = False
        rounding_digits = 3
        if column_name[-1] == '?':
            optional = True
            column_name = column_name[:-1]
        if column_name[-2] == '.' and column_name[-1] in '0123456789':
            rounding_digits = int(column_name[-1])
            column_name = column_name[:-2]

        if column_name not in columns:
            print(f'[Error] Unknown column name: {column_name}, please correct the leaderboard option in your config file')
            sys.exit(1)
        h, c = columns[column_name]
        if optional and c.count(c[0]) == len(c):
            continue
        is_number = all(isinstance(x, (int, float)) for x in c if x is not None)
        is_int = is_number and all([x.is_integer() for x in c if x is not None])

        table.add_column(h, c, f'.{rounding_digits}f' if is_number and not is_int else '', 'decimal' if column_name != 'name' else 'left')

    table.show(args.output or cfg['show_output'])


def mode_info() -> None:
    bots = load_db()
    if args.name not in bots:
        print(f'[Error] Bot {args.name} doesn\'t exist')
        sys.exit(1)
    games = load_all_games()

    ranking = sorted(bots.values(), key=lambda b: b.mu-3*b.sigma, reverse=True)

    enemy_bots = [b for b in ranking if b.name != args.name]

    wins = {b.name: 0 for b in enemy_bots}
    losses = {b.name: 0 for b in enemy_bots}
    draws = {b.name: 0 for b in enemy_bots}
    
    for game in games:
        for i, p1 in enumerate(game.players):
            if p1 == args.name:
                for j, p2 in enumerate(game.players):
                    if i == j:
                        continue
                    wins[p2]   += int(game.ranks[i]  < game.ranks[j])
                    losses[p2] += int(game.ranks[i]  > game.ranks[j])
                    draws[p2]  += int(game.ranks[i] == game.ranks[j])

    headers = ['Pos', 'Name', 'Score', '%', 'Wins', 'Losses', 'Draws']
    columns = []
    columns.append(list(range(1, 1+len(enemy_bots))))
    columns.append([b.name for b in enemy_bots])
    columns.append([b.mu-3*b.sigma for b in enemy_bots])
    columns.append([(wins[b.name] + draws[b.name]*0.5) / (wins[b.name] + losses[b.name] + draws[b.name]) * 100 if wins[b.name] + losses[b.name] + draws[b.name] else 0 for b in enemy_bots])
    columns.append([wins[b.name] for b in enemy_bots])
    columns.append([losses[b.name] for b in enemy_bots])
    columns.append([draws[b.name] for b in enemy_bots])

    table = DataTable()
    for i, column in enumerate(columns):
        is_number = all(isinstance(x, (int, float)) for x in column)
        is_int = is_number and all([x.is_integer() for x in column if x is not None])
        table.add_column(headers[i], column, f'.3f' if is_number and not is_int else '', 'decimal' if i != 1 else 'left')

    table.show(args.output or cfg['show_output'])


def mode_db_recreate() -> None:
    if not unique_run():
        print('[Error] Psyleague server is already running, aborting...')
        sys.exit(1)

    if not args.force_confirm:
        print('WARNING:')
        print('THIS WILL OVERWRITE ALL OF THE DATA IN THE .DB FILE!')
        print('Type "yes" to continue: ', end='')
        if input().strip().upper() != 'YES':
            print('Aborting...')
            return

    bots = load_db()
    games = load_all_games()
    if args.shuffle:
        print('Shuffling games...')
        random.shuffle(games)
        save_all_games(games)
    print('Recalculating ranking...')
    bots = recalculate_ranking(bots, games)
    save_db(bots)


def mode_db_verify() -> None:
    bots = load_db()
    games = load_all_games()

    if not bots and not games:
        print('No bots and no games in the database, nothing to verify!')
        return

    issues = []

    missing_bots = set()
    for game in games:
        for player in game.players:
            if player not in bots and player not in missing_bots:
                missing_bots.add(player)
                issues.append(f'Game {game} contains an unknown player: {player}')
                print(issues[-1])

    if not issues:
        verify_bots = recalculate_ranking(bots, games)
        for b in bots.values():
            if b.games != verify_bots[b.name].games:
                issues.append(f'Bot {b.name} has {b.games} games in the database and {verify_bots[b.name].games} games in the recalculated ranking')
            if b.errors != verify_bots[b.name].errors:
                issues.append(f'Bot {b.name} has {b.errors} errors in the database and {verify_bots[b.name].errors} errors in the recalculated ranking')
            if b.mu != verify_bots[b.name].mu:
                issues.append(f'Bot {b.name} has mu {b.mu} in the database and {verify_bots[b.name].mu} in the recalculated ranking')

    if not issues:
        print('Found no issues in the database')
        return
    
    print('Found the following issues in the database:')
    for issue in issues:
        print(issue)    


#endregion


def test():
    bots = load_db()
    print(json.dumps(bots[0].__dict__))



def _main() -> None:
    parser = argparse.ArgumentParser(description='Local league system for bot contests\nMore help available at https://github.com/FakePsyho/psyleague \nYou can type psyleague mode --help for more information about specific mode', formatter_class=argparse.RawTextHelpFormatter)
    parser.set_defaults(func=None)
    subparsers = parser.add_subparsers(title='modes')
    
    parser_config = subparsers.add_parser('config', aliases=['c'], help='creates a new config file in the current directory')
    parser_config.set_defaults(func=mode_config)
    
    parser_run = subparsers.add_parser('run', aliases=['r'], help='start psyleague and spin-up all of the workers')
    parser_run.set_defaults(func=mode_run)
    parser_run.add_argument('-g', '--games', type=int, default=None, help='if specified, number of games to run after which psyleague should finish running')
    parser_run.add_argument('-w', '--workers', type=int, default=None, help='number of workers to use (overrides config file)')
    parser_run.add_argument('-s', '--silent', action='store_true', help='turns off all of the messages')
    parser_run.add_argument('-v', '--verbose', action='store_true', help='shows extra information, good for debugging')

    parser_bot = subparsers.add_parser('bot', aliases=['b'], help='commands related to adding/stopping/updating bots')
    parser_bot.set_defaults(func=mode_bot)
    parser_bot.add_argument('cmd', choices=['add', 'update', 'stop', 'remove'], help='add a new bot/update bot/stop currently active bot (no more played games)/remove a bot along with games (recalculates ranking)')
    parser_bot.add_argument('name', help='name of the bot')
    parser_bot.add_argument('-s', '--src', type=str, default=None, help='source file, if not provided defaults to NAME (used in add)')
    parser_bot.add_argument('--setup', type=str, default=None, help='overrides cmd_bot_setup from the config file (used in add)')
    parser_bot.add_argument('-d', '--description', type=str, default='n/a', help='description of the bot (used in add/update)')
    parser_bot.add_argument('-n', '--new-name', type=str, default=None, help='new name of the bot (used in update)')

    parser_show = subparsers.add_parser('show', aliases=['s'], help='shows the current ranking for all bots\n-s/-f/-i/-x requires recalculating ranking (which may take a while)')
    parser_show.set_defaults(func=mode_show)
    parser_show.add_argument('-a', '--active', action='store_true', help='shows only active bots')
    parser_show.add_argument('-o', '--output', choices=['table','csv','json'], default=None, help='output format of the ranking')
    parser_show.add_argument('-m', '--model', choices=['trueskill', 'openskill', 'global'], default=None, help='recalculates ranking using a different model')
    parser_show.add_argument('-f', '--filters', type=str, default=None, nargs='+', help='recalculates ranking after filtering the games)')
    parser_show.add_argument('-i', '--include', type=str, default=None, nargs='+', help='recalculates ranking including only bots matching specified regexes (note: only games with all bots present are considered)')
    parser_show.add_argument('-x', '--exclude', type=str, default=None, nargs='+', help='recalculates ranking excluding bots matching specified regexes')
    parser_show.add_argument('-s', '--resample', type=int, default=None, help='recalculates ranking using bootstrapping')
    parser_show_xgroup = parser_show.add_mutually_exclusive_group()
    parser_show_xgroup.add_argument('-b', '--best', type=int, default=None, help='limits ranking to the best X bots')
    parser_show_xgroup.add_argument('-r', '--recent', type=int, default=None, help='limits ranking to the most recent X bots')

    parser_info = subparsers.add_parser('info', aliases=['i'], help='shows info about a particular bot')
    parser_info.set_defaults(func=mode_info)
    parser_info.add_argument('name', help='name of the bot')
    parser_info.add_argument('-a', '--active', action='store_true', help='shows only active bots')
    parser_info.add_argument('-o', '--output', choices=['table','csv','json'], default=None, help='output format of the ranking')

    parser_db = subparsers.add_parser('db', aliases=['d'], help='operations on the database')
    parser_db_subparsers = parser_db.add_subparsers(title='db modes', required=True)
    parser_db_recreate = parser_db_subparsers.add_parser('recreate', help='recreates the ranking using all of the games (warning: not useful unless your db file is corrupted)')
    parser_db_recreate.set_defaults(func=mode_db_recreate)
    parser_db_recreate.add_argument('-y', '--force-confirm', action='store_true', help='skips the confirmation prompt')
    parser_db_recreate.add_argument('-s', '--shuffle', action='store_true', help='shuffles the games before recalculating the ranking')
    parser_db_verify = parser_db_subparsers.add_parser('verify', help='verifies the integrity of the database')
    parser_db_verify.set_defaults(func=mode_db_verify)

    global args
    args = parser.parse_args()

    # test()
    
    if not args.func:
        parser.print_help()
        return 
    
    # load config
    if args.func != mode_config:
        if not os.path.exists(CONFIG_FILE):
            print('Missing config file, please run "psyleague config" to create a config file in you current directory')
            return
    
        global cfg
        cfg = load_config(CONFIG_FILE)
        assert cfg['version'] == __version__, 'Version of the config file doesn\'t match psyleague version'
    
    args.func()


if __name__ == '__main__':
    _main()
