# Freeze manifest for this repo. Nothing Python to freeze here; the line below
# names the C module so a build that includes this manifest compiles it
# (MicroPython 1.29 c_module()).
c_module(".")  # this directory holds the micropython.cmake / micropython.mk for the C half
