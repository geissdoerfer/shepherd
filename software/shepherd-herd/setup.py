from setuptools import setup

requirements = ["click", "fabric", "numpy", "pyYAML"]

setup(
    name="shepherd_herd",
    version="1.0",
    description=(
        "Synchronized Energy Harvesting"
        "Emulator and Recorder frontend"
    ),
    packages=["shepherd_herd"],
    classifiers=[
        # How mature is this project? Common values are
        #   3 - Alpha
        #   4 - Beta
        #   5 - Production/Stable
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "Intended Audience :: Information Technology",
        "Programming Language :: Python :: 3",
    ],
    install_requires=requirements,
    setup_requires=["pytest-runner"],
    tests_require=["pytest>=3.9"],
    author="Kai Geissdoerfer",
    author_email="kai dot geissdoerfer at tu-dresden dot de",
    entry_points={"console_scripts": ["shepherd-herd=shepherd_herd:main"]},
)
