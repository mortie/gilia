#!/usr/bin/env python3

import fs
import shutil
import os
import subprocess
import time
import sys
import argparse
from matplotlib import pyplot as plt
from tqdm import tqdm

def bench_one_iter(name):
    start = time.time()
    subprocess.run(
            ["./gilia/build/gilia", f"benches/{name}/{name}.g"],
            check=True, capture_output=True)
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
    subprocess.run(["git", "clone", "../..", "gilia"], check=True)

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

    n = len(commits)
    for commit in tqdm(commits, desc="Benchmarking"):
        n -= 1
        parts = commit.split(" ")
        cid = parts[0]

        try:
            subprocess.check_output(["make", "-C", "gilia", "clean"])
            subprocess.check_output(["make", "-C", "gilia", "-j", "8"])
        except subprocess.CalledProcessError:
            continue

        for name in names:
            subprocess.run(["git", "-C", "gilia", "checkout", "-q", cid], check=True)
            try:
                score = bench_one_version(name)
                xs[name].append(n)
                ys[name].append(score)
            except subprocess.CalledProcessError:
                print("Error on commit", commit, file=sys.stderr)

    print("Writing output...", file=sys.stderr)
    try:
        os.mkdir("output")
    except: pass
    for name in names:
        for x, ydots in tqdm(list(zip(xs[name], ys[name])), desc=name):
            for y in ydots:
                plt.scatter(x, y, c="blue")
        plt.title(name)
        plt.ylim(ymin=0)
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

bench_all_versions(args.benchmarks, count=args.n)
