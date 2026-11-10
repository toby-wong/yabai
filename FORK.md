# Toby's yabai fork

This fork adds native macOS tab groups to upstream yabai. Enable them in your
`yabairc`:

```sh
yabai -m config window_native_tabs on
```

The selected tab occupies the group's existing tile. Opening, switching, and
closing tabs hand over that tile instead of splitting the layout. A detached
tab becomes a separate window. The setting is off by default.

Normal Finder windows must be managed for this to tile Finder tabs. Remove an
all-Finder `manage=off` rule if you want Finder tiled; retain narrower rules for
copy, connection, preferences, and other dialogs.

## Where the changes came from

The native-tab implementation is Christian-SC26's refinement of jpatrolla's
work, described in [upstream issue 68](https://github.com/asmvik/yabai/issues/68#issuecomment-6632044024).
The original authorship and source commit IDs are retained in the imported
commit messages.

Imported from `Christian-SC26/yabai`:

| Source commit | Change |
| --- | --- |
| `04764aa03f5078cd9b651b15f3931fb0f559ff26` | Native tabs share one BSP or stack tile |
| `039fae3c8d395589d90df496b1e04ed10f19db5e` | Accurate tab detection, existing-tab discovery, disable behavior, insertion targets |
| `3cbb2a5be4a7740aa3b79872fdbfb3a855205ed2` | Floating groups remain floating |
| `f20dd3bc391156fe1fe8af90912a6f873d67c194` | Active opacity for new tabs |
| `9db612ce7780e67887c4aec2746f05aee7b53428` | Restore opacity when a hidden tab becomes visible |
| `ac2525e328ec2e4fb84297ce773c378b6d6fcd23` | Avoid opacity flashes during tab switches |
| `3876b2f59b8fdad085adf3735411862e209d58d5` | Ignore hidden tabs' stale Space IDs when following an application |

The final fix is adapted to upstream's `process_manager_active_space_for_psn`.
This fork retains upstream's application activation and Mission Control code
rather than importing the other fork's separate redesigns. The native-tab
regressions are adapted to upstream's smaller test harness.

Two patches from the previously installed build are preserved:

- `09ce58ad389668853bcde074a3f4c892a3dd0e34`: newer macOS versions use the
  newest supported code paths, including window notifications on macOS 27.
- `66d2ab7aaa2faece51580647fc79b6b63a9ce67a`: apps that hide a window instead of
  closing it relinquish its tile and recover it when shown again.

The visibility handlers are reconciled with native tabs. With native tabs
enabled, ordinary hide handling waits for the 250 ms tab-event pairing interval
and rechecks the window. Tab handovers themselves do not wait for that timer.

## Install a prebuilt release with Homebrew

After removing or unlinking a conflicting yabai installation:

```sh
brew tap toby-wong/yabai https://github.com/toby-wong/yabai.git
brew install toby-wong/yabai/yabai
```

The formula downloads the checksummed universal binary from this repository's
GitHub Releases. It does not compile yabai locally. It uses the normal Homebrew
binary path and works with `yabai --start-service`.

Update with:

```sh
brew update
brew upgrade toby-wong/yabai/yabai
yabai --restart-service
```

## Install without Homebrew

Download the `.tar.gz` and `SHA256SUMS` from a release, verify them with
`shasum -a 256 -c SHA256SUMS`, and extract the archive. Install `archive/bin/yabai`
at a stable executable path, then use `yabai --start-service`. Keep the previous
binary for rollback. Each release includes its exact source commit in
`archive/build-info.json`, and `yabai --version` reports the fork build ID.

Both options use the same Apple Silicon/Intel binary. Builds are ad-hoc signed;
they are not Developer ID signed or notarized. macOS may request Accessibility
access after replacing the binary. This fork does not change SIP or enable the
scripting addition.

## Automatic upstream updates and releases

`Sync upstream and publish` runs every six hours, on changes pushed to `master`,
and on manual dispatch. It fetches `asmvik/yabai`'s `master` and merges it into
the fork in a temporary Actions checkout. It never resets or force-pushes the
fork over its patches.

The workflow tests merge behavior, builds and runs the native-tab/visibility
regressions, and compiles a universal release on a GitHub-hosted Mac. Only then
does it push the merge, publish a release with SHA-256 checksums and source
metadata, and update `Formula/yabai.rb`. Unchanged scheduled checks skip builds.
A merge conflict, failed check, or concurrent change to `master` stops
publication; inspect the failed Actions run and resolve it before retrying.

The build and publication happen in the same workflow because a push made
using `GITHUB_TOKEN` does not trigger another push workflow. The generated
formula update is excluded from push-triggered builds. Workflow actions are
pinned to commit IDs.

This automatically maintains the fork and makes new binaries available.
Updating an installed Mac is a separate `brew upgrade` or release-download step.
Future native-tab changes in Christian-SC26's fork require review and import;
automatic upstream sync follows asmvik's mainline.

## Validation and rollback

`make -C tests` exercises layout and visibility using synthetic window and Space
IDs. It does not move the user's windows. `python3 -m unittest discover -s tests`
checks successful merges, no-change runs, and conflict recovery in disposable
Git repositories. The existing `tests/integration/hide_on_close.sh` is an
explicit interactive test and is not run by the unit-test workflow.

Finder tab switching, Merge All Windows, tab detachment, floating tabs, and
native fullscreen still need verification on the destination Mac. They depend
on private macOS APIs and cannot be proven by compilation or synthetic tests.

To disable the feature, use `yabai -m config window_native_tabs off` and remove
the enabling config line. For a binary rollback, stop the service, restore the
previous executable or install an earlier prebuilt release, and start the
service again. Back up `yabairc`, the launch agent, and the old binary before the
first migration.
