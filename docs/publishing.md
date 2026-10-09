# Manual GitHub publication

No remote was created and nothing was pushed during reconstruction.

Because the recovered code has no project license and two experiments contain
uncertain third-party provenance, the conservative workflow is to publish
privately first, review `LICENSES.md`, then make it public for employers only
after resolving those points.

From PowerShell:

```powershell
cd "PATH\TO\wireless-midi-adapter"
git init
git branch -M main
git add .
git commit -m "Reconstruct ESP32 wireless MIDI adapter from recovered artifacts"
git status

gh auth status
gh repo create YOUR_GITHUB_USER/wireless-midi-adapter --private --source . --remote origin
git push -u origin main
```

To create a public repository immediately, use `--public` instead of
`--private`. To change an already-pushed private repository after the provenance
review:

```powershell
gh repo edit YOUR_GITHUB_USER/wireless-midi-adapter --visibility public --accept-visibility-change-consequences
```

Before sharing with employers, verify the rendered Mermaid diagram, ensure no
build outputs are staged, and pin the repository on the GitHub profile.
