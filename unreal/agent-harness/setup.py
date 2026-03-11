#!/usr/bin/env python3
"""Setup script for Unreal Engine CLI."""

from setuptools import setup, find_packages

setup(
    name="cli-anything-unreal",
    version="0.1.0",
    description="Command-line interface for Unreal Engine 5",
    author="CLI-Anything",
    packages=find_packages(),
    install_requires=[
        "click>=8.0",
    ],
    entry_points={
        "console_scripts": [
            "ue-cli=cli_anything.unreal.unreal_cli:cli",
            "ue-cli-full=cli_anything.unreal.unreal_cli_full:cli",
        ],
    },
    python_requires=">=3.10",
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Libraries",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
    ],
)
