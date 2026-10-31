# Publishing to the native GitHub Wiki

[Documentation index](./README.md) · [Project README](../README.md)

**Two different destinations exist:**

- Version-controlled source documentation is in this repository's [docs/](./README.md).
- The actual GitHub Wiki is at https://github.com/RobertRupa/KinConductor/wiki
  and its Git remote is https://github.com/RobertRupa/KinConductor.wiki.git.

Changes to docs/ **do not automatically appear** on GitHub Wiki. Use the
standard-library-only [publishing script](../scripts/publish-wiki.py) to
generate and synchronize the Markdown pages.

## What gets published

The publisher creates or updates **16 native GitHub Wiki pages**: Home,
the GitHub sidebar (_Sidebar.md), and 14 chapters: Getting Started,
Build Profiles, Build and Flashing, First Start, WebGUI, Settings Reference,
HTTP API, Hardware, Peripherals, Current Measurement, Architecture,
Troubleshooting, Releases, and Manufacturer Resources.

The content is copied from the checked-out source documentation. Relative
Markdown chapter links are rewritten as links to native Wiki pages; images
remain in the main repository and point at a source-commit-pinned raw URL.
Links to other repository files also point to the source commit. Existing
Wiki pages with other names are left alone; managed page names (including
Home and _Sidebar) are overwritten by the source content.

## One-time preparation on an authenticated Ubuntu workstation

You need Python 3, Git, GitHub repository write access and connectivity to
github.com. GitHub Wiki must be enabled in repository settings. When GitHub
has not initialized the Wiki git repository yet, visit the repository's
**Wiki** tab and create its first Home page using the browser once.

For HTTPS Git credentials, either use an existing credential helper or
install the GitHub CLI, then run:

~~~bash
gh auth login
gh auth setup-git
git config --global user.name "Your GitHub author name"
git config --global user.email "your-verified-or-noreply-github-email"
~~~

Replace the Git author identity with your own, or skip the two git config
lines if your user.name and user.email are already configured.
Do not put access tokens directly into commands or tracked files.

## Publish from the current documentation branch

~~~bash
# In a clean directory, clone the branch containing the updated documentation.
git clone --single-branch \
  --branch docs/installation-configuration-webgui-20261031 \
  https://github.com/RobertRupa/KinConductor.git KinConductor-wiki-sync
cd KinConductor-wiki-sync

# Dry run: generate Markdown under ignored dist/wiki-preview/.
python3 scripts/publish-wiki.py

# Actual publication: clone separate Wiki Git repo, commit and push 16 pages.
python3 scripts/publish-wiki.py --publish
~~~

Alternatively, after merging the documentation PR into master, run the same
Python commands from a current local master checkout. Run the publisher again
whenever source docs change. It only creates a Wiki commit when generated
files differ; it does not force-push or erase unrelated Wiki pages.

## Verification and safety

Open https://github.com/RobertRupa/KinConductor/wiki and check Home,
the sidebar, the WebGUI screenshot URLs, the setup commands, and the
settings-reference table. A source-docs commit or a successful preview is
**not** evidence that the remote Wiki was published.

If Git cloning returns a 404, check that the Wiki was initialized and that
the account has permission to access it. If Git push fails, check GitHub
authentication and permission, not the firmware. The script does not flash
hardware or change firmware code. It does update the generated Wiki pages
under the account that runs it.
