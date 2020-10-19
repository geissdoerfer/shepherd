import os
from setuptools import setup

requirements = [
    "click",
    "click-config-file",
    "numpy",
    "python-periphery==1.1.2",
    "scipy",
    "zerorpc",
    "invoke",
    "h5py",
]

# We are installing the DBUS module to build the docs, but the C libraries
# required to build dbus aren't available on RTD, so we need to exclude it
# from the installed dependencies here, and mock it for import in docs/conf.py
# using the autodoc_mock_imports parameter:
if not os.getenv("READTHEDOCS"):
    requirements.append("dbus-python")

setup(
    name="shepherd",
    version="0.2.2",
    description=("Synchronized Energy Harvesting" "Emulator and Recorder"),
    packages=["shepherd"],
    classifiers=[
        # How mature is this project? Common values are
        #   3 - Alpha
        #   4 - Beta
        #   5 - Production/Stable
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Intended Audience :: Information Technology",
        "Programming Language :: Python :: 3",
    ],
    install_requires=requirements,
    setup_requires=["pytest-runner"],
    tests_require=["pytest>=3.9", "pyfakefs", "pytest-timeout", "pytest-click"],
    author="Kai Geissdoerfer",
    author_email="kai dot geissdoerfer at tu-dresden dot de",
    entry_points={"console_scripts": ["shepherd-sheep=shepherd.cli:cli"]},
)
