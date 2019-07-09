from setuptools import setup

requirements = [
    "click",
    "click-config-file",
    "numpy",
    "python-periphery",
    "scipy",
    "zerorpc",
    "invoke",
    "h5py",
]

setup(
    name="shepherd",
    version="0.0.1",
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
    tests_require=[
        "pytest>=3.9",
        "pyfakefs",
        "pytest-timeout",
        "pytest-click",
    ],
    author="Kai Geissdoerfer",
    author_email="kai dot geissdoerfer at tu-dresden dot de",
    entry_points={"console_scripts": ["shepherd-sheep=shepherd.cli:cli"]},
)
