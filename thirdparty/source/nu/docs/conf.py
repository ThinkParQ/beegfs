#!/usr/bin/env python3
# -*- coding: utf-8 -*-

needs_sphinx = '1.0'

project = 'nu'
copyright = '2016, dhivael'
author = 'dhivael'
release = '0.1.0'
version = release
language = 'en'

master_doc = 'index'
primary_domain = 'cpp'
source_suffix = '.rst'
highlight_language = 'cpp'
html_theme = 'agogo'
html_show_sourcelink = False
html_static_path = ['_static']
pygments_style = 'sphinx'

cpp_index_common_prefix = ['nu::']

nitpick_ignore = [
        ('cpp:typeOrConcept', 'std'),
        ('cpp:typeOrConcept', 'std::exception'),
        ('cpp:typeOrConcept', 'std::error_code'),
]
