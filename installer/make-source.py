# Packs the source of this version for the AGPL offer; run before compiling the installer:
#   py installer\make-source.py
# JUCE and clap-juce-extensions are not packed: the workflow pins their exact versions, the README says where.
import pathlib, zipfile

root = pathlib.Path(__file__).resolve().parent.parent
version = "0.2.0"
keep = ["LICENSE", "THIRD-PARTY-LICENSES.txt", "README.md", "USER_MANUAL.html", "index.html", ".gitignore", ".github", "installer",
        "plugin/CMakeLists.txt", "plugin/build.cmd", "plugin/Resources", "plugin/Source", "plugin/test", "plugin/tools", "plugin/train"]
out = root / "plugin/build/installer" / f"gensexuell-{version}-source.zip"
out.parent.mkdir(parents=True, exist_ok=True)

with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for item in keep:
        path = root / item
        for f in [path] if path.is_file() else sorted(p for p in path.rglob("*") if p.is_file()):
            if f.name != "dataset.bin":
                z.write(f, f"gensexuell-{version}/{f.relative_to(root).as_posix()}")

names = zipfile.ZipFile(out).namelist()
assert any(n.endswith("plugin/Source/Engine.h") for n in names) and any(n.endswith("/LICENSE") for n in names)
print(out, len(names), "files", out.stat().st_size // 1024, "KB")
