import pathlib
from setuptools import setup

# The directory containing this file
HERE = pathlib.Path(__file__).parent

# The text of the README file
README = (HERE / "README.md").read_text()

requirements = ["click", "fabric", "numpy", "pyYAML"]

setup(
    name="shepherd_herd",
    version="0.0.6",
    description="Synchronized Energy Harvesting Emulator and Recorder CLI",
    long_description=README,
    long_description_content_type="text/markdown",
    packages=["shepherd_herd"],
    license="MIT",
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
    author="Kai Geissdoerfer",
    author_email="kai.geissdoerfer@tu-dresden.de",
    entry_points={"console_scripts": ["shepherd-herd=shepherd_herd:main"]},
)
