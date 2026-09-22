"""Use Clang for the host-only coverage environment."""
import shutil
import subprocess
import sys

Import("env")

def tool(name):
    if sys.platform == "darwin":
        return subprocess.check_output(["xcrun", "--find", name], text=True).strip()
    path = shutil.which(name)
    if not path:
        raise RuntimeError("Coverage requires LLVM/Clang: missing " + name)
    return path

env.Replace(CC=tool("clang"), CXX=tool("clang++"), LINK=tool("clang++"))
env.Append(LINKFLAGS=["-fprofile-instr-generate"])
if sys.platform == "darwin":
    sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    env.Append(CCFLAGS=["-isysroot", sdk], LINKFLAGS=["-isysroot", sdk])
