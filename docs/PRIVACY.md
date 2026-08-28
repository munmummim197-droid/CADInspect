# Privacy policy

This policy describes CADInspect runtime version `0.1.0-dev` as represented by
the current public source repository. It does not describe GitHub, SignPath,
download sites, operating-system services, or other software used separately by
the user.

## Network communication

This program will not transfer any information to other networked systems unless
specifically requested by the user or the person installing or operating it.

The current CADInspect GUI and CLI do not implement network-transfer features.
The runtime does not link Qt Network and does not contain an application network
client. Repository auditing found no telemetry, analytics, automatic update
check, crash upload, external API call, or user-data upload path.

Build tools such as Git, vcpkg, CMake dependency downloads, GitHub Actions, and a
web browser may access networks while obtaining source, dependencies, or release
files. Those operations are outside the installed CADInspect runtime and are
governed by the policies of the respective tools and services.

## Local data processing

CADInspect processes STEP/STP input files locally. It may read file paths,
timestamps, sizes, hashes, assembly structure, geometry, and other model metadata
needed for comparison. It can write JSON and CSV reports to a location selected
by the user.

Canonical reports contain full input paths, SHA-256 values, and model metadata.
These reports may identify confidential filenames, directory layouts, or CAD
content. Users should review or redact reports before sharing them with another
person or system.

## Storage and retention

CADInspect does not operate a server-side account or retention service. Input
files, reports, caches, and application files remain on systems controlled by the
user. Removing locally generated reports or input files is the user's decision.

## Installation and removal

The Windows installer places the application under Program Files, registers an
uninstall entry, creates a Start Menu shortcut, registers an App Paths entry, and
can create a desktop shortcut when selected. It does not automatically launch the
application after installation. Windows elevation is requested because the
installer writes to machine-wide locations.

CADInspect can be removed using the Windows Installed Apps / Programs interface
or its Inno Setup uninstaller. Portable packages can be removed by deleting the
extracted package after closing the application.

## Third-party components

Packaged Qt, Open CASCADE Technology, Microsoft runtime, and transitive library
files retain their upstream licenses. CADInspect does not use their optional
network services in the current runtime composition. See
[`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) and the exact license files
distributed under `licenses/`.

## Changes to this policy

Any future feature that transmits data must be documented here before release.
If transfer is not strictly initiated to a destination specified by the user or
installer/operator, the installer must display the applicable policy and offer
the controls required by the current SignPath Foundation conditions.
