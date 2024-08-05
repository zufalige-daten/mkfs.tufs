'mkfs.xfs'

project_root = './'
binary_out = 'mkfs.xfs'
source_dir = 'src'
object_dir = 'obj'
include_dir = 'include'
object_ext = 'o'
object_main = 'main.o'

{
	include_type = 'lib local'
	include_local = '#include "%s"'
	include_lib = '#include <%s>'
	source_ext = 'cpp'
	compiler_command = 'g++ -c -m64 -o {output} {input} -I {include}'
}

linker_command = 'g++ -m64 -o {output} {input}'
