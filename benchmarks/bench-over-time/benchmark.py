#!/usr/bin/env python3

import shutil
import os
import subprocess
import time
import sys
import argparse
import signal
from matplotlib import pyplot as plt
from tqdm import tqdm

terminate = False

def handle_sigint(sig, frame):
    global terminate
    if terminate:
        print("\n===> Terminating non-gracefully.", file=sys.stderr)
        exit(1)
    else:
        print("\n===> Terminating ASAP...", file=sys.stderr)
        terminate = True

def run_command(args):
    try:
        ret = subprocess.run(
                args, check=True, preexec_fn=lambda: signal.signal(signal.SIGINT, signal.SIG_IGN),
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as ex:
        print("\nCommand failed:", args, file=sys.stderr)
        print("Output:", file=sys.stderr)
        sys.stderr.buffer.write(ex.output);
        raise ex

def bench_one_iter(name):
    start = time.time()
    run_command(["./gilia/build/gilia", f"benches/{name}/{name}.g"])
    return time.time() - start

def bench_one_version(name):
    times = []

    start = time.time()
    for i in tqdm(range(0, 3), desc="Warmup", leave=False):
        bench_one_iter(name)
    time_per_iter = (time.time() - start) / 3

    iters = min(max(int(3 / time_per_iter), 10), 50)

    for i in tqdm(range(0, iters), desc=name, leave=False):
        times.append(bench_one_iter(name))

    return times

def bench_all_versions(names, count=None):
    try:
        shutil.rmtree("gilia")
    except: pass
    run_command(["git", "clone", "../..", "gilia"])

    xs = {}
    ys = {}
    for name in names:
        xs[name] = []
        ys[name] = []

    commits = (subprocess
            .check_output([
                "git", "-C", "gilia", "log", "--pretty=oneline",
                "6bb9e478ea225bef82a20bc86f6761d523120d64..HEAD"])
            .decode("utf-8").strip().split("\n"))

    if count != None:
        commits = commits[0:count]

    n = 1
    for commit in tqdm(commits, desc="Benchmarking"):
        n -= 1
        parts = commit.split(" ")
        cid = parts[0]

        try:
            run_command(["make", "-C", "gilia", "clean"])
            run_command(["make", "-C", "gilia", "-j", "8"])
        except subprocess.CalledProcessError:
            continue

        for name in names:
            run_command(["git", "-C", "gilia", "checkout", "-q", cid])
            try:
                score = bench_one_version(name)
                xs[name].append(str(n))
                ys[name].append(score)
            except subprocess.CalledProcessError:
                print("\nError on commit", commit, file=sys.stderr)

        if terminate:
            break

    print("Writing output...", file=sys.stderr)
    try:
        os.mkdir("output")
    except: pass
    for name in names:
        lst = list(zip(xs[name], ys[name]))
        lst.reverse()
        for x, ydots in tqdm(lst, desc=name):
            for y in ydots:
                plt.scatter(x, y, c="blue")
        plt.title(name)
        plt.ylim(ymin=0)
        plt.grid(axis='y')
        plt.savefig(f"output/{name}.png")
        plt.cla()

parser = argparse.ArgumentParser()
parser.add_argument("benchmarks", type=str, nargs="+")
parser.add_argument("-n", type=int, default=None)

args = parser.parse_args()

for name in args.benchmarks:
    if not os.path.exists(f"benches/{name}/{name}.g"):
        print(f"No such benchmark: {name}")
        exit(1)

signal.signal(signal.SIGINT, handle_sigint)
bench_all_versions(args.benchmarks, count=args.n)
