require 'mkmf'
dir_config('rbfuse_lib.so')
if have_library('fuse_ino64') || have_library('fuse') 
  create_makefile('rbfuse_lib')
else
  puts "No FUSE install available"
end
