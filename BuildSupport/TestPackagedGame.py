"""Build, relocate, play, quit, resume and finish the Release Twin Rooms game on Windows."""
import ctypes
from ctypes import wintypes
import json
import os
import shutil
from pathlib import Path
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "Bin" / "ReleaseX64"
USER = ctypes.WinDLL("user32", use_last_error=True)
CALLBACK = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
USER.EnumWindows.argtypes = [CALLBACK, wintypes.LPARAM]
USER.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
USER.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
USER.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]


def window(pid, title):
    result = []
    @CALLBACK
    def visit(hwnd, _):
        owner = wintypes.DWORD()
        USER.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid:
            name = ctypes.create_unicode_buffer(512)
            USER.GetWindowTextW(hwnd, name, len(name))
            if name.value == title:
                result.append(hwnd)
        return True
    USER.EnumWindows(visit, 0)
    return result[0] if result else None


def wait_for(process, find, label, seconds=60):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if process.poll() is not None:
            raise RuntimeError(f"Game exited during {label} with {process.returncode}")
        result = find()
        if result:
            return result
        time.sleep(.05)
    raise TimeoutError(label)


def key(hwnd, vk, scan, down=True):
    if not USER.PostMessageW(hwnd, 0x100 if down else 0x101, vk, (scan << 16) | 1 | (0 if down else 0xC0000000)):
        raise ctypes.WinError(ctypes.get_last_error())


def tap(hwnd, vk, scan):
    key(hwnd, vk, scan)
    time.sleep(.15)
    key(hwnd, vk, scan, False)


def play(exe, work, saves, resume):
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    env = dict(os.environ, NUCLEAR_SAVE_ROOT=str(saves))
    log = work / ("resume.log" if resume else "new-game.log")
    with log.open("w") as output:
        game = subprocess.Popen([exe], cwd=work, env=env, stdout=output, stderr=output, startupinfo=startup)
        try:
            title = wait_for(game, lambda: window(game.pid, "Twin Rooms - Title"), "title")
            if resume:
                tap(title, 9, 15)
            tap(title, 13, 28)
            level = "Atrium" if resume else "Gallery"
            hwnd = wait_for(game, lambda: window(game.pid, "Twin Rooms - " + level), level)
            if not resume:
                tap(hwnd, ord("P"), 25)
                tap(hwnd, 9, 15)
                tap(hwnd, 13, 28)
                time.sleep(.2)
                tap(hwnd, ord("I"), 23)
                wait_for(game, lambda: any(isinstance(json.loads(p.read_text()), dict) for p in saves.rglob("*.json")), "save new controls")
                tap(hwnd, 13, 28)
            key(hwnd, ord("I"), 23)
            if resume:
                def won():
                    return any(json.loads(p.read_text()) == "" for p in saves.rglob("*.json"))
                wait_for(game, won, "win and clear checkpoint")
            else:
                wait_for(game, lambda: window(game.pid, "Twin Rooms - Atrium"), "advance to Atrium")
            key(hwnd, ord("I"), 23, False)
            USER.PostMessageW(hwnd, 0x10, 0, 0)
            if game.wait(timeout=20) != 0:
                raise RuntimeError("Unclean game shutdown")
        finally:
            if game.poll() is None:
                game.terminate()
                game.wait()


def export(project, target, work):
    return subprocess.run(
        [BIN / "Nuclear.Editor.exe", "--build-game", project, BIN, ROOT / "Assets", target],
        cwd=work, capture_output=True, timeout=120)


def check_model_dependencies(work):
    source = work / "model source"
    source.mkdir()
    (source / "triangle.obj").write_text(
        "mtllib triangle.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl Wall\nf 1 2 3\n")
    (source / "triangle.mtl").write_text("newmtl Wall\nmap_Kd surface.png\n")
    shutil.copyfile(ROOT / "Assets/NuclearEngine/Editor/NuclearLogo.png", source / "surface.png")
    project = source / "model.nuclearproject"
    project.write_text('NuclearProject 3\nproject "Model fixture"\nassembly ""\nlevels 0\nprefabs 0\nmap "model.NMap"\nend\n')
    map_file = source / "model.NMap"
    scene = (ROOT / "Samples/TwinRooms/Gallery.NMap").read_text()
    def with_model(name):
        map_file.write_text(scene.replace('"plane" "default"', f'"file:{name}" "file:{name}"'))
    with_model("triangle.obj")
    target = work / "model export"
    result = export(project, target, work)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors="replace"))
    for asset in ("triangle.obj", "triangle.mtl", "surface.png"):
        packaged = list(target.rglob(asset))
        if len(packaged) != 1 or packaged[0].read_bytes() != (source / asset).read_bytes():
            raise RuntimeError(f"Lost model dependency {asset}")
    (source / "surface.png").unlink()
    rejected = work / "rejected export"
    result = export(project, rejected, work)
    if result.returncode != 1 or rejected.exists():
        raise RuntimeError("An incomplete texture dependency did not fail export cleanly")
    skin = json.loads((ROOT / "Nuclear.Editor/Tests/Fixtures/TwoClips.gltf").read_text())
    del skin["skins"][0]["inverseBindMatrices"]
    (source / "unsupported.gltf").write_text(json.dumps(skin))
    with_model("unsupported.gltf")
    result = export(project, rejected, work)
    if result.returncode != 1 or rejected.exists():
        raise RuntimeError(f"Unsupported skin did not fail cleanly: {result.returncode}")


def main():
    with tempfile.TemporaryDirectory(prefix="nuclear-package-") as temporary:
        work = Path(temporary)
        target = work / "export"
        result = export(ROOT / "Samples/TwinRooms/TwinRooms.nuclearproject", target, work)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace") or f"Export exited {result.returncode}")
        relocated = work / "relocated game"
        target.rename(relocated)
        if [p.name for p in relocated.rglob("*.nuclearproject")] != ["Game.nuclearproject"]:
            raise RuntimeError("Expected exactly one main project in the package")
        maps = list(relocated.rglob("*.NMap"))
        if len(maps) != 3 or any(not p.read_text().startswith("NuclearMap ") for p in maps):
            raise RuntimeError("Startup and both playable rooms must be packaged as map assets")
        exe = relocated / "Game.exe"
        for resume in (False, True):
            try:
                play(exe, work, work / "saves", resume)
            except Exception:
                for log in work.glob("*.log"):
                    print(log.name, log.read_text(errors="replace")[-3000:])
                diagnostic = work / "play-error.txt"
                if diagnostic.exists():
                    print(diagnostic.read_text(errors="replace"))
                raise
        check_model_dependencies(work)
        print("Model, material and texture packaging and failed-export cleanup passed.")
        print("Relocated Windows Release game passed menu, pause, persistent key rebinding, prefab triggers, level change, quit, resume and win.")


if __name__ == "__main__":
    main()
