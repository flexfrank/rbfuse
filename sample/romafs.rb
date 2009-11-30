# encoding: utf-8
require "rubygems"
require "rbfuse"
require "roma/client"

require "json"

class RomaFS < RbFuse::FuseDir
  def initialize(roma_node)
    #@table={}
    @table =Roma::Client::RomaClient.new(roma_node)
    ent=dir_entries("/")
    if !ent
      set_dir("/",[])
    end
    @open_entries={}
  end

  def to_dirkey(path)
    "dir:"+path
  end

  def to_filekey(path)
    "file:"+path
  end

  def get_dir(path)
    @table[to_dirkey(path)]
  end

  def get_file(path)
    @table[to_filekey(path)]
  end

  def set_file(path,str)
    @table[to_filekey(path)]=str
  end

  def set_dir(path,ary)
    @table[to_dirkey(path)]=JSON.dump(ary)
  end

  def dir_entries(path)
    val=@table[to_dirkey(path)]
    val ? JSON.load(val) : nil
  end

  def delete_file(path)
    if(get_file(path))
      @table.delete(to_filekey(path))
      dirname=File.dirname(path)
      set_dir(dirname,dir_entries(dirname)-[File.basename(path)])
    end
  end

  def file?(path)
    file=get_file(path)
    !!file
  end

  def directory?(path)
    !!get_dir(path)
  end

  def size(path)
    file=get_file(path)
    if file
      file.bytesize
    else
      0
    end
  end

    


 
  public 
  def readdir(path)
    ents=JSON.load(get_dir(path))
    ents||[]
  end

  def getattr(path)
   if(file?(path))
     stat=RbFuse::Stat.file
     stat.size=size(path)
     stat
   elsif(directory?(path))
     RbFuse::Stat.dir
   else
     nil
   end
 
  end

  def open(path,mode,handle)
    buf=nil
    if mode=~/r/
      buf=get_file(path)
    end
    buf||=""
    buf.encode("ASCII-8bit")

    @open_entries[handle]=[mode,buf]
    true
  end
  def read(path,off,size,handle)
    @open_entries[handle][1][off,size]
  end
  def write(path,off,buf,handle)
    @open_entries[handle][1][off,buf.bytesize]=buf
  end
  def close(path,handle)
    return nil unless @open_entries[handle]
    set_file(path,@open_entries[handle][1])

    @open_entries.delete(handle)
    dir=File.dirname(path)
    files=JSON.load(get_dir(dir))
    set_dir(dir,files|[File.basename(path)])
  end

  
  def unlink(path)
    delete_file(path)
    true
  end
  def mkdir(path,mode)
    p mode
    @table[to_dirkey(path)]=JSON.dump([])
    filename=File.basename(path)
    parent_dir=File.dirname(path)
    files=JSON.load(get_dir(parent_dir))|[filename]
    @table[to_dirkey(File.dirname(path))]=JSON.dump(files)
    true
  end
  
  def rmdir(path)
    dirname=File.dirname(path)
    basename=File.basename(path)
    set_dir(dirname,JSON.load(get_dir(dirname))-[basename])
    @table.delete(to_dirkey(path))
  end

end

mountdir=ARGV.shift
RbFuse.debug=true
RbFuse.set_root(RomaFS.new(ARGV.to_a))
RbFuse.mount_under mountdir
RbFuse.run
RbFuse.unmount
