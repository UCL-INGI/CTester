# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# http://www.sphinx-doc.org/en/master/config

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os
import sys
import sphinx_rtd_theme
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = 'CTester'
copyright = '2019, UCL-INGI'
author = 'UCL-INGI'
release = '1.0.0'


# -- General configuration ---------------------------------------------------

extensions = [
    #"breathe",
    "sphinx_rtd_theme"
]

templates_path = ['_templates']

source_suffix = ['.rst']

master_doc = 'index'

exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', ]
exclude_patterns += ['full_doc/*.rst', 'doxygendoc.rst'] # Disable it for now

primary_domain = 'c'

highlight_language = 'c'

pygments_style = None

manpages_url = 'http://man7.org/linux/man-pages/man{section}/{page}.{section}.html'

html_theme = 'sphinx_rtd_theme'

html_static_path = ['_static']

# C Autodoc (a.k.a. hawkmoth) configuration
cautodoc_root = os.path.abspath('../student/CTester')
cautodoc_compat = 'javadoc-basic'

# breathe configuration
breathe_projects = { "CTester": "./doxygen_output/xml/"}
breathe_default_project = "CTester"

