#!/usr/bin/python3
import subprocess as sp
import os
import argparse
import shlex

#version 1 proof of concept, needs to remove a lot of repeated code

parser = argparse.ArgumentParser(description="Like GNU make")
parser.add_argument("-d", "--directory")
parser.add_argument("--src")
parser.add_argument("--include")
parser.add_argument("--compile")
parser.add_argument("--link")
args = parser.parse_args()

folder = args.directory or os.getcwd()
srcFolder = args.src or "/src/"
includeFolder = args.include or "/include/"

def get_includes(file):
    #cant use -P because its not in all grep installs
    proc = sp.Popen(["grep", '^#include "', file], stdout=sp.PIPE)
    result = proc.communicate()[0]
    if result is None: return []

    return [x.split('"')[1] for x in result.decode("utf-8").splitlines()]


#todo make recurisve
def all_includes(project_root, file):
    includes = get_includes(file)
    for include in includes:
        includes2 = get_includes(project_root + includeFolder + include);
        for i2 in includes2:
            if not i2 in includes:
                includes.append(i2);
    return includes

def time_modified(file):
    try:
        stat = os.stat(file)
        return stat.st_mtime
    except FileNotFoundError:
        return 0

def check_if_needs_compile(project_root, source):
    obj_time = time_modified(project_root + srcFolder + source_to_object(source))
    if(obj_time == 0):
        print("No object")
        return True
    source_time = time_modified(project_root + srcFolder + source)
    if(obj_time < source_time):
        return True
    for include in all_includes(project_root, project_root + srcFolder + source):
        include_time = time_modified(project_root + includeFolder + include)
        if(obj_time < include_time):
            return True
    return False

def is_source_file(file):
    return file.endswith(".cpp")

def is_object_file(file):
    return file.endswith(".o")

def get_objects(project_root):
    sources = os.listdir(project_root + srcFolder)
    return list(filter(is_object_file, sources))

def get_sources(project_root):
    sources = os.listdir(project_root + srcFolder)
    return list(filter(is_source_file, sources))

def source_to_object(source):
    return ".".join(source.split(".")[:-1]) + ".o"

def print_sources_to_compile():
    sources = get_sources(folder)
    for source in sources:
        if(check_if_needs_compile(folder, source)): print(source)

def compile_source(folder, compiler, source):
    abs_path = folder + srcFolder + source
    compiler_args = shlex.split(
        compiler.replace("@src", abs_path).replace("@out", source_to_object(abs_path))
    )
    proc = sp.Popen(compiler_args)
    print(" ".join(compiler_args))
    result = proc.communicate()[0]
    print(result)

def compile_sources(compiler):
    sources = get_sources(folder)
    compiled_something = False
    for source in sources:
        if(check_if_needs_compile(folder, source)):
            print("Compiling", source)
            compile_source(folder, compiler, source)
            compiled_something = True

    if(not compiled_something):
        print("Nothing to compile")

def link_objects(linker):
    objects = get_objects(folder)
    objects = list(map(lambda o: folder + srcFolder + o, objects))
    linker_args = shlex.split(
        linker.replace("@objs", " ".join(objects))
    )
    proc = sp.Popen(linker_args)
    print("Linking objects")
    result = proc.communicate()[0]

def main():
    if(args.compile):
        compile_sources(args.compile)
        link_objects(args.link)
    else:
        print_sources_to_compile()

main()
