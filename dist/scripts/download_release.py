#!/usr/bin/env python3

import argparse
from io import BytesIO
import os
import sys
import re
from zipfile import ZipFile

import requests
import yaml


def get_all_workflow_artifacts(s, run_id):
    r = s.get(
        f"https://api.github.com/repos/bryceishunter/leapdesk-kvm/actions/runs/{run_id}/artifacts"
    )
    return {a["name"]: a["archive_download_url"] for a in r.json()["artifacts"]}


def find_by_re(items, regex):
    for i in items:
        if re.match(regex, i) is not None:
            return i
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("run_url", type=str)
    parser.add_argument("dest_directory", type=str)
    parser.add_argument("version", type=str)
    args = parser.parse_args()

    version = args.version

    workflow_run_id = args.run_url.split("/")[-1]

    with open(os.path.expanduser("~/.config/hub")) as f:
        conf = yaml.safe_load(f)
        token = conf["github.com"][0]["oauth_token"]

    s = requests.Session()
    s.headers.update({"Authorization": "token " + token})

    received_artifacts = get_all_workflow_artifacts(s, workflow_run_id)

    artifacts_config = {
        "leapdesk-kvm-deb-debian-12": (
            "leapdesk-kvm_.*_amd64.deb",
            f"Leapdesk_{version}_debian12_amd64.deb",
        ),
        "leapdesk-kvm-deb-ubuntu-20-04": (
            "leapdesk-kvm_.*_amd64.deb",
            f"Leapdesk_{version}_ubuntu_20-04_amd64.deb",
        ),
        "leapdesk-kvm-deb-ubuntu-22-04": (
            "leapdesk-kvm_.*_amd64.deb",
            f"Leapdesk_{version}_ubuntu_22-04_amd64.deb",
        ),
        "leapdesk-kvm-deb-ubuntu-24-04": (
            "leapdesk-kvm_.*_amd64.deb",
            f"Leapdesk_{version}_ubuntu_24-04_amd64.deb",
        ),
        "leapdesk-kvm-deb-ubuntu-24-10": (
            "leapdesk-kvm_.*_amd64.deb",
            f"Leapdesk_{version}_ubuntu_24-10_amd64.deb",
        ),
        "leapdesk-kvm-rpms-fedora": (
            "x86_64/leapdesk-kvm-.*.fc40.x86_64.rpm",
            f"Leapdesk_{version}_fedora_fc40_x86_64.rpm",
        ),
        "leapdesk-kvm-flatpak-x86_64": (
            "leapdesk-kvm.flatpak",
            f"Leapdesk_{version}_linux_x86_64.flatpak",
        ),
        "macOS-Apple_Silicon-installer": (
            "Leapdesk-.*-release.dmg",
            f"Leapdesk_{version}_macos_AppleSilicon.dmg",
        ),
        "macOS-x86_64-installer": (
            "Leapdesk-.*-release.dmg",
            f"Leapdesk_{version}_macos_x86_64.dmg",
        ),
        "windows-installer-Windows Qt5": (
            "LeapdeskSetup-.*-release.exe",
            f"Leapdesk_{version}_windows_qt5.exe",
        ),
        "windows-installer-Windows Qt6": (
            "LeapdeskSetup-.*-release.exe",
            f"Leapdesk_{version}_windows_qt6.exe",
        ),
    }

    os.makedirs(args.dest_directory, exist_ok=True)

    errors = []

    for name, artifact_config in artifacts_config.items():
        download_url = received_artifacts.get(name, None)
        if download_url is None:
            errors.append(f"Artifact {name} not found in Github artifacts")
            continue

        print(f"{name}: Downloading from {download_url}")
        r = s.get(download_url)
        if r.status_code != 200:
            errors.append(f"{name}: Failed to download {download_url}")
            continue

        r.raise_for_status()
        zipfile = ZipFile(BytesIO(r.content))

        zip_filename_re, dest_filename = artifact_config
        zip_filename = find_by_re(zipfile.namelist(), zip_filename_re)
        if zip_filename is None:
            errors.append(
                f"{name}: {zip_filename_re} not in downloaded zip: {zipfile.namelist()}"
            )
            continue

        dest_path = os.path.join(args.dest_directory, dest_filename)
        with open(dest_path, "wb") as out_f:
            print(f"{name}: Writing to {dest_path}")
            out_f.write(zipfile.open(zip_filename).read())

    if errors:
        print("Got errors:")
        for e in errors:
            print(e)
        sys.exit(1)


if __name__ == "__main__":
    main()
