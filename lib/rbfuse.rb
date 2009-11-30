# RbFuse.rb
#
# The ruby portion of RbFuse main library
#
# This includes helper functions, common uses, etc.

require 'rbfuse_lib'

module RbFuse
  @running = true
  def self.run
    @mounted_at=Time.now
    fd = RbFuse.fuse_fd
    io = IO.for_fd(fd)
    while @running
      IO.select([io])
      self.process
    end
  end
  def self.unmount
    system("fusermount -u #{@mountpoint}")
  end
  def self.exit
    @running = false
  end
  Stat=Struct.new(:perm,:filetype,:size,:nlink,:uid,:gid,:mtime,:atime,:ctime)

  class Stat
    def self.init_time
      @@init_time||=Time.now
      @@init_time
    end
    def initialize
      self.size=0
      self.nlink=1
      self.uid=RbFuse.uid
      self.gid=RbFuse.gid
      self.atime=self.class.init_time
      self.mtime=self.class.init_time
      self.ctime=self.class.init_time
    end
    def self.file
       result=self.new
       result.perm=0666
       result.filetype=RbFuse::S_IFREG
       result
    end
    def self.dir
      result=self.new
      result.size=4096
      result.perm=0777
      result.filetype=RbFuse::S_IFDIR
      result
    end
  end

  class FuseDir
    def split_path(path)
      cur, *rest = path.scan(/[^\/]+/)
      if rest.empty?
        [ cur, nil ]
      else
        [ cur, File.join(rest) ]
      end
    end
    def scan_path(path)
      path.scan(/[^\/]+/)
    end

    def rename(path,destpath)
      
      fhr=Object.new
      stat=self.stat(path)
      return nil unless stat
      self.open(path,"r",fhr)
      file=self.read(path,0,stat.size,fhr)
      self.close(path,fhr)

      fh=Object.new
      self.open(destpath,"w",fh)
      self.write(destpath,0,file,fh)
      self.close(destpath,fh)
      self.delete(path)
      true
    end


    def truncate(path,len)
      fh=Object.new
      open(path,"r",fh)
      str=read(path,0,len,fh)
      close(path,fh)
      return nil unless str
      if(str.bytesize<len)
        str+="\0"*len-str.bytesize
      else
        str=str[0,len]
      end
      fh2=Object.new
      open(path,"w",fh2)
      write(path,0,str,fh2)
      close(path,fh2) 
      true
    end


    def create(path,mode)
      handle=Object.new
      self.open(path,"w",handle)
      self.write(path,0,"",handle)
      self.close(path,handle)
    end

  end
end
