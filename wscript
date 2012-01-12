#
# PS: This is a python file

blddir = 'build'
VERSION = '0.0.1'

src_files = ['src/hello.cc']
target = 'hello_node'


def set_options(opt):
    # Makes C++ a configurable option
    opt.tool_options('compiler_cxx')


def configure(conf):
    # Checking for program g++ or c++
    conf.check_tool('compiler_cxx')

    # Check that the node_addon plugin is properly installed
    # See {prefix}/lib/node/wafadmin/Tools/node_addon.py
    conf.check_tool('node_addon')


def build(bld):
    """
        Define the dependencies/configuration for this build target
            cxx:         g++ or c++
            shlib:       linking library
            node_addon:  include specific node configuration, ex: adds -I/usr/local/include/node
                         to the options. See "man g++" for more
    """
    obj = bld.new_task_gen('cxx', 'shlib', 'node_addon')
    obj.target = target
    obj.source = src_files
