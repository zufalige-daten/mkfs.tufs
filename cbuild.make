'mkfs.tufs'

{
	compiler_command = 'g++ -c -o {output} -D_LARGEFILE64_SOURCE {input} -I {include}'
	include_type = 'lib local'
	include_lib = '#include <%s>'
	include_local = '#include "%s"'
	source_ext = 'cc'
}

link = 'g++'
binary_out = 'bin/mkfs.tufs'

