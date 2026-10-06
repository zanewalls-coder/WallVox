# Builds manifest.json for the Hub. Each plugin's version = number of commits touching its source.
import json, subprocess, sys
repo, out = sys.argv[1], sys.argv[2]
rel = f"https://github.com/{repo}/releases/download/latest/"
raw = f"https://raw.githubusercontent.com/{repo}/main/"
def count(paths):
    return int(subprocess.check_output(["git", "rev-list", "--count", "HEAD", "--", *paths]).strip() or 0)
plugins = []
for p in json.load(open("hub/plugins.json")):
    plugins.append({ "id": p["id"], "name": p["name"], "tagline": p["tagline"], "color": p["color"],
                     "version": count(p["src"]), "url": rel + p["zip"], "bundles": p["bundles"],
                     "presets": raw + p["presets"] if p["presets"] else "", "presetDir": p["presetDir"] })
manifest = { "hub": { "version": count(["WallHub", "Shared"]), "url": rel + "WallHub-Mac.zip" }, "plugins": plugins }
json.dump(manifest, open(out, "w"), indent=2)
print(json.dumps(manifest, indent=2))
