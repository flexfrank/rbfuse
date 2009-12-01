/* ruby-fuse
 *
 * A Ruby module to interact with the FUSE userland filesystem in
 * a Rubyish way.
 */

/* #define DEBUG 
/**/

#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ruby.h>
#include <unistd.h>


#ifdef DEBUG
#include <stdarg.h>
#endif

#define RF_READDIR  "readdir"
#define RF_OPEN     "open"
#define RF_READ     "read"
#define RF_WRITE    "write"
#define RF_CLOSE    "close"
#define RF_UNLINK   "unlink"
#define RF_MKDIR    "mkdir"
#define RF_RMDIR    "rmdir"
#define RF_TRUNCATE "truncate"
#define RF_RENAME   "rename"
#define RF_CREATE   "create"
#define RF_GETATTR  "getattr"


#include "rbfuse_fuse.h"

/* init_time
 *
 * All files will have a modified time equal to this. */
time_t init_time;



/* Ruby Constants constants */
static VALUE cRbFuse      = Qnil; /* RbFuse class */
static VALUE cFSException = Qnil; /* Our Exception. */
static VALUE FuseRoot     = Qnil; /* The root object we call */
static int debugMode=0;






 
static int writable(const char* path){
  return 1;
}
static int deletable(const char* path){
  return 1;
}
static int mkdirable(const char* path){
  return 1;
}
static int rmdirable(const char* path){
  return 1; 
}

typedef unsigned long int (*rbfunc)();

/* debug()
 *
 * If #define DEBUG is enabled, then this acts as a printf to stderr
 */
#ifdef DEBUG
static inline void
debug(const char *msg,...) {
  va_list ap;
  va_start(ap,msg);
  vfprintf(stderr,msg,ap);
}
#else
#define debug 

#endif

static inline void dp(const char* message,const char* path){
  if(debugMode){
    printf("--- %s (%s)\n",message,path);
  }
}
static VALUE
rf_funcall(VALUE recv,const char *methname, VALUE arg) ;


static VALUE
get_stat(const char* path){
  VALUE args=rb_ary_new();
  rb_ary_push(args,rb_str_new2(path));
  return rf_funcall(FuseRoot,RF_GETATTR,args);
}
static mode_t 
get_stat_filetype(VALUE stat){
  VALUE ft=rf_funcall(stat,"filetype",Qnil);
  if(FIXNUM_P(ft)){
    return FIX2LONG(ft);
  }else{
    return 0;
  }
}

static VALUE handle_table(){
  return rb_iv_get(cRbFuse,"@handles");
}

static int
stat_is_dir(VALUE stat){
  mode_t ftype=get_stat_filetype(stat);
  return ftype==S_IFDIR; 
}
static int
stat_is_reg(VALUE stat){
  mode_t ftype=get_stat_filetype(stat);
  return ftype==S_IFREG; 
}




static VALUE
rf_protected_call(VALUE args) {
  VALUE recv = rb_ary_shift(args);
  ID to_call = SYM2ID(rb_ary_shift(args));
  return rb_apply(recv,to_call,args);
}

static VALUE
rf_rescue(VALUE data,VALUE exception){
  if(debugMode){
    rb_p(ID2SYM(rb_intern("error")));
    rb_p(exception);
    rb_p(rb_funcall(exception,rb_intern("backtrace"),0));
  }
  return Qnil;
}



static VALUE
rf_funcall(VALUE recv,const char *methname, VALUE arg) {
  VALUE result;
  VALUE methargs;

  ID method=rb_intern(methname);

  if (!rb_respond_to(recv,method)) {
    debug("not respond %s",methname);
    return Qnil;
  }

  if (arg == Qnil) {
    debug("    root.%s\n", methname );
  } else {
    debug("    root.%s(...)\n", methname);
  }

  if (TYPE(arg) == T_ARRAY) {
    methargs = arg;
  } else if (arg != Qnil) {
    methargs = rb_ary_new();
    rb_ary_push(methargs,arg);
  } else {
    methargs = rb_ary_new();
  }

  rb_ary_unshift(methargs,ID2SYM(method));
  rb_ary_unshift(methargs,recv);

  /* Set up the call and make it. */
  result = rb_rescue(rf_protected_call, methargs, rf_rescue, Qnil);
 

  return result;
}




/* rf_getattr
 *
 * Used when: 'ls', and before opening a file.
 *
 * FuseFS will call: directory? and file? on FuseRoot
 *   to determine if the path in question is pointing
 *   at a directory or file. The permissions attributes
 *   will be 777 (dirs) and 666 (files) xor'd with FuseFS.umask
 */

static int
rf_getattr2(const char*path,struct stat* stbuf){

  dp("rf_getattr2", path );
  /* Zero out the stat buffer */
  memset(stbuf, 0, sizeof(struct stat));

  if (strcmp(path,"/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_size = 4096;
    stbuf->st_nlink = 1;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_mtime = init_time;
    stbuf->st_atime = init_time;
    stbuf->st_ctime = init_time;
    return 0;
  }


  VALUE stat=get_stat(path);
  if(RTEST(stat)){
     
    VALUE perm=rf_funcall(stat,"perm",Qnil);
    perm=rb_funcall(stat,rb_intern("perm"),0);
    if(!FIXNUM_P(perm))return -ENOENT;
    mode_t perm_m=FIX2LONG(perm);
   
    mode_t filetype_m=get_stat_filetype(stat);
    if(filetype_m==0)return -ENOENT;

    stbuf->st_mode=perm_m|filetype_m;

    
    VALUE size=rf_funcall(stat,"size",Qnil);
    if(!FIXNUM_P(size))return -ENOENT;
    stbuf->st_size=FIX2LONG(size);
 
    VALUE nlink=rf_funcall(stat,"nlink",Qnil);
    if(!FIXNUM_P(nlink))return -ENOENT;
    stbuf->st_nlink=FIX2LONG(nlink);

    VALUE uid=rf_funcall(stat,"uid",Qnil);
    if(!FIXNUM_P(uid))return -ENOENT;
    stbuf->st_uid=FIX2INT(uid);

    VALUE gid=rf_funcall(stat,"gid",Qnil);
    if(!FIXNUM_P(gid))return -ENOENT;
    stbuf->st_gid=FIX2INT(gid);

    VALUE atime=rf_funcall(stat,"atime",Qnil);
    VALUE atimei=rf_funcall(atime,"to_i",Qnil);
    if(!RTEST(atimei))return -ENOENT;
    stbuf->st_atime=NUM2LONG(atimei);

    VALUE mtime=rf_funcall(stat,"mtime",Qnil);
    VALUE mtimei=rf_funcall(mtime,"to_i",Qnil);
    if(!RTEST(mtimei))return -ENOENT;
    stbuf->st_mtime=NUM2LONG(mtimei);

    VALUE ctime=rf_funcall(stat,"ctime",Qnil);
    VALUE ctimei=rf_funcall(ctime,"to_i",Qnil);
    if(!RTEST(ctimei))return -ENOENT;
    stbuf->st_ctime=NUM2LONG(ctimei);
   
    return 0;
  }else{
    return -ENOENT;
  }
}



/* rf_readdir
 *
 * Used when: 'ls'
 *
 * FuseFS will call: 'directory?' on FuseRoot with the given path
 *   as an argument. If the return value is true, then it will in turn
 *   call 'contents' and expects to receive an array of file contents.
 *
 * '.' and '..' are automatically added, so the programmer does not
 *   need to worry about those.
 */
static int
rf_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
           off_t offset, struct fuse_file_info *fi) {
  VALUE retval;

  dp("rf_readdir", path );

  /* This is what fuse does to turn off 'unused' warnings. */
  (void) offset;
  (void) fi;

  /* FuseRoot must exist */
  if (FuseRoot == Qnil) {
    if (!strcmp(path,"/")) {
      filler(buf,".", NULL, 0);
      filler(buf,"..", NULL, 0);
      return 0;
    }
    return -ENOENT;
  }

  if (strcmp(path,"/") != 0) {
    debug("  Checking is_directory? ...");
    retval = rf_funcall(FuseRoot,"directory?",rb_str_new2(path));

    if (!RTEST(retval)) {
      debug(" no.\n");
      return -ENOENT;
    }
    debug(" yes.\n");
  }
 
  /* These two are Always in a directory */
  filler(buf,".", NULL, 0);
  filler(buf,"..", NULL, 0);

  VALUE args=rb_ary_new();
  rb_ary_push(args,rb_str_new2(path));
  retval = rf_funcall(FuseRoot, RF_READDIR,args);
  if (!RTEST(retval)) {
    return 0;
  }
  if (TYPE(retval) != T_ARRAY) {
    return 0;
  }

  size_t size=RARRAY_LEN(retval);
  VALUE* vptr=RARRAY_PTR(retval);
  size_t i;
  for(i=0;i<size;i++ ) {
    VALUE ent=vptr[i];

    if (TYPE(ent) != T_STRING)
      continue;

    filler(buf,StringValueCStr(ent),NULL,0);
  }
  return 0;
}

/* rf_mknod
 *
 * Used when: This is called when a file is created.
 *
 * Note that this is actually almost useless to FuseFS, so all we do is check
 *   if a path is writable? and if so, return true. The open() will do the
 *   actual work of creating the file.
 */
static int
rf_mknod(const char *path, mode_t umode, dev_t rdev) {

  dp("rf_mknod", path);
  /* Make sure it's not already open. */

  /* We ONLY permit regular files. No blocks, characters, fifos, etc. */
  debug("  Checking if an IFREG is requested ...");
  if (!S_ISREG(umode)) {
    debug(" no.\n");
    return -EACCES;
  }
  debug(" yes.\n");

  VALUE stat=get_stat(path);
 
  debug("  Checking if it's a file ..." );
  if (stat_is_reg(stat)) {
    debug(" yes.\n");
    return -EEXIST;
  }
  debug(" no.\n");

  /* Is this writable to */
  if (!writable(path)) {
    return -EACCES;
  }

  debug("call create method\n");
  VALUE args=rb_ary_new();
  VALUE pv=rb_str_new2(path);
  rb_ary_push(args,pv);
  rb_ary_push(args,INT2FIX(umode));
  rf_funcall(FuseRoot,RF_CREATE,args);


  return 0;
}

/* rf_open
 *
 * Used when: A file is opened for read or write.
 *
 * If called to open a file for reading, then FuseFS will call "read_file" on
 *   FuseRoot, and store the results into the linked list of "opened_file"
 *   structures, so as to provide the same file for mmap, all excutes of
 *   read(), and preventing more than one call to FuseRoot.
 *
 * If called on a file opened for writing, FuseFS will first double check
 *   if the file is writable to by calling "writable?" on FuseRoot, passing
 *   the path. If the return value is a truth value, it will create an entry
 *   into the opened_file list, flagged as for writing.
 *
 * If called with any other set of flags, this will return -ENOPERM, since
 *   FuseFS does not (currently) need to support anything other than direct
 *   read and write.
 */
static int
rf_open(const char *path, struct fuse_file_info *fi) {
  char open_opts[4], *optr;

  dp("rf_open", path);

 

  optr = open_opts;
  switch (fi->flags & 3) {
  case 0:
    *(optr++) = 'r';
    break;
  case 1:
    *(optr++) = 'w';
    break;
  case 2:
    *(optr++) = 'w';
    *(optr++) = 'r';
    break;
  default:
    debug("Opening a file with something other than rd, wr, or rdwr?");
  }
  if (fi->flags & O_APPEND)
    *(optr++) = 'a';
  *(optr) = '\0';

  
  VALUE handle=rb_class_new_instance(0,NULL,rb_cObject);

  VALUE h_table=handle_table();
  rb_hash_aset(h_table,handle,Qtrue);
  fi->fh=handle;

  VALUE args=rb_ary_new();
  rb_ary_push(args,rb_str_new2(path));
  rb_ary_push(args,rb_str_new2(open_opts));
  rb_ary_push(args,handle);
  if (RTEST(rf_funcall(FuseRoot,RF_OPEN,args))) {
    return 0;
  }else{
    return -ENOENT;
  }}

/* rf_release
 *
 * Used when: A file is no longer being read or written to.
 *
 * If release is called on a written file, FuseFS will call 'write_to' on
 *   FuseRoot, passing the path and contents of the file. It will then
 *   clear the file information from the in-memory file storage that
 *   FuseFS uses to prevent FuseRoot from receiving incomplete files.
 *
 * If called on a file opened for reading, FuseFS will just clear the
 *   in-memory copy of the return value from rf_open.
 */
static int
rf_release(const char *path, struct fuse_file_info *fi) {
   dp("rf_release", path);

  VALUE handle=fi->fh;
  


  /* If it's opened for raw read/write, call raw_close */
  VALUE args=rb_ary_new();
  rb_ary_push(args,rb_str_new2(path));
  rb_ary_push(args,handle);
  rf_funcall(FuseRoot,RF_CLOSE,args);
  VALUE h_table=handle_table();
  rb_hash_delete(h_table,handle);

  return 0;
 
}

/* rf_touch
 *
 * Used when: A program tries to modify the file's times.
 *
 * We use this for a neat side-effect thingy. When a file is touched, we
 * call the "touch" method. i.e: "touch button" would call
 * "FuseRoot.touch('/button')" and something *can* happen. =).
 */
static int
rf_touch(const char *path, struct utimbuf *ignore) {
  dp("rf_touch", path);
  rf_funcall(FuseRoot,"touch",rb_str_new2(path));
  return 0;
}

/* rf_rename
 *
 * Used when: a file is renamed.
 *
 * When FuseFS receives a rename command, it really just removes the old file
 *   and creates the new file with the same contents.
 */
static int
rf_rename(const char *path, const char *dest) {
  VALUE pathv=rb_str_new2(path);
  VALUE destv=rb_str_new2(dest);

  VALUE args=rb_ary_new();
  rb_ary_push(args,pathv);
  rb_ary_push(args,destv);
  VALUE ret=rf_funcall(FuseRoot,RF_RENAME,args);
  if(RTEST(ret)){
    return 0;
  }else{
    return -EACCES;
  }

}

/* rf_unlink
 *
 * Used when: a file is removed.
 *
 * This calls can_remove? and remove() on FuseRoot.
 */
static int
rf_unlink(const char *path) {
  dp("unlink",path);
  /* Does it exist to be removed? */
  debug("  Checking if it exists...");
  VALUE stat=get_stat(path);
  if (!stat_is_reg(stat)) {
    debug(" no.\n");
    return -ENOENT;
  }
  debug(" yes.\n");

  /* Can we remove it? */
  debug("  Checking if we can remove it...");
  if (!deletable(path)) {
    debug(" yes.\n");
    return -EACCES;
  }
  debug(" no.\n");
 
  /* Ok, remove it! */
  debug("  Removing it.\n");
  VALUE args=rb_ary_new();
  rb_ary_push(args,rb_str_new2(path));
  rf_funcall(FuseRoot,RF_UNLINK,args);
  
  return 0;

}

/* rf_truncate
 *
 * Used when: a file is truncated.
 *
 * If this is an existing file?, that is writable? to, then FuseFS will
 *   read the file, truncate it, and call write_to with the new value.
 */
static int
rf_truncate(const char *path, off_t length) {
  dp( "rf_truncate", path);

  

  /* Does it exist to be truncated? */
  VALUE stat=get_stat(path);
  if(!stat_is_reg(stat)){
    return -ENOENT;
  }
  
  if(rb_respond_to(FuseRoot,rb_intern(RF_TRUNCATE))){
    VALUE args=rb_ary_new();
    rb_ary_push(args,rb_str_new2(path));
    rb_ary_push(args,LONG2NUM(length));
    rf_funcall(FuseRoot,RF_TRUNCATE,args);
    return 0;
  }

 
  return -EACCES;

}

/* rf_mkdir
 *
 * Used when: A user calls 'mkdir'
 *
 * This calls can_mkdir? and mkdir() on FuseRoot.
 */
static int
rf_mkdir(const char *path, mode_t mode) {
  dp("rf_mkdir",path);
  /* Does it exist? */

  VALUE stat=get_stat(path);
  if(RTEST(stat)) return -EEXIST;

  /* Can we mkdir it? */
  if (!mkdirable(path))
    return -EACCES;
 
  VALUE args=rb_ary_new();
  rb_ary_push(args,rb_str_new2(path));
  rb_ary_push(args,INT2FIX(mode));
  /* Ok, mkdir it! */
  rf_funcall(FuseRoot,RF_MKDIR,args);
  return 0;
 

}

/* rf_rmdir
 *
 * Used when: A user calls 'rmdir'
 *
 * This calls can_rmdir? and rmdir() on FuseRoot.
 */
static int
rf_rmdir(const char *path) {
  dp("rf_rmdir",path);
  /* Does it exist? */
  VALUE stat=get_stat(path);
  if(RTEST(stat)){
    if(!stat_is_dir(stat))  return -ENOTDIR;
  }else{
    return -ENOENT;
  }

  /* Can we rmdir it? */
  if (!rmdirable(path))
    return -EACCES;
 
  /* Ok, rmdir it! */
  rf_funcall(FuseRoot,RF_RMDIR,rb_str_new2(path));

  return 0;

}

/* rf_write
 *
 * Used when: a file is written to by the user.
 *
 * This does not access FuseRoot at all. Instead, it appends the written
 *   data to the opened_file entry, growing its memory usage if necessary.
 */
static int
rf_write(const char *path, const char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi) {
    dp("rf_write",path);


  debug( "  Offset is %d\n", offset );



  /* Make sure it's open for write ... */
  /* If it's opened for raw read/write, call raw_write */
    /* raw read */
    VALUE args = rb_ary_new();
    debug(" yes.\n");
    rb_ary_push(args,rb_str_new2(path));
    rb_ary_push(args,INT2NUM(offset));
    rb_ary_push(args,rb_str_new(buf,size));
    rb_ary_push(args,fi->fh);
    rf_funcall(FuseRoot,RF_WRITE,args);
  return (int)size;

}

/* rf_read
 *
 * Used when: A file opened by rf_open is read.
 *
 * In most cases, this does not access FuseRoot at all. It merely reads from
 * the already-read 'file' that is saved in the opened_file list.
 *
 * For files opened with raw_open, it calls raw_read
 */
static int
rf_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
    dp( "rf_read", path );


    /* If it's opened for raw read/write, call raw_read */
    /* raw read */
    VALUE args = rb_ary_new();
    rb_ary_push(args,rb_str_new2(path));
    rb_ary_push(args,INT2NUM(offset));
    rb_ary_push(args,INT2NUM(size));
    rb_ary_push(args,fi->fh);
    VALUE ret = rf_funcall(FuseRoot,RF_READ,args);
    if (!RTEST(ret))
      return 0;
    if (TYPE(ret) != T_STRING)
      return 0;
    size_t len=RSTRING_LEN(ret);
    if(size<len)len=size;
    memcpy(buf, RSTRING_PTR(ret), len);
    return (int)len;

 
}

static int
rf_fsyncdir(const char * path, int p, struct fuse_file_info *fi)
{
  return 0;
}

static int 
rf_utime(const char *path, struct utimbuf *ubuf)
{
  return 0;
}

static int
rf_statfs(const char *path, struct statvfs *buf)
{
  buf->f_frsize=1024;
  buf->f_namemax=255;
  buf->f_bsize=1024;
  buf->f_blocks=1024*1024;
  buf->f_bavail=1024*1024;
  buf->f_bfree=1024*1024;
  return 0;
}

/* rf_oper
 *
 * Used for: FUSE utilizes this to call operations at the appropriate time.
 *
 * This is utilized by rf_mount
 */
static struct fuse_operations rf_oper = {
    .getattr   = rf_getattr2,
    .readdir   = rf_readdir,
    .mknod     = rf_mknod,
    .unlink    = rf_unlink,
    .mkdir     = rf_mkdir,
    .rmdir     = rf_rmdir,
    .truncate  = rf_truncate,
    .rename    = rf_rename,
    .open      = rf_open,
    .release   = rf_release,
    .utime     = rf_touch,
    .read      = rf_read,
    .write     = rf_write,
    .fsyncdir  = rf_fsyncdir,
    .utime     = rf_utime,
    .statfs    = rf_statfs,
};

/* rf_set_root
 *
 * Used by: FuseFS.set_root
 *
 * This defines FuseRoot, which is the crux of FuseFS. It is required to
 *   have the methods "directory?" "file?" "contents" "writable?" "read_file"
 *   and "write_to"
 */
VALUE
rf_set_root(VALUE self, VALUE rootval) {
  if (self != cRbFuse) {
    rb_raise(cFSException,"Error: 'set_root' called outside of FuseFS?!");
    return Qnil;
  }

  rb_iv_set(cRbFuse,"@root",rootval);
  FuseRoot = rootval;
  return Qtrue;
}



/* rf_mount_to
 *
 * Used by: FuseFS.mount_to(dir)
 *
 * FuseFS.mount_to(dir) calls FUSE to mount FuseFS under the given directory.
 */
VALUE
rf_mount_to(int argc, VALUE *argv, VALUE self) {
  struct fuse_args *opts;
  VALUE mountpoint;
  int i;
  char *cur;

  if (self != cRbFuse) {
    rb_raise(cFSException,"Error: 'mount_to' called outside of FuseFS?!");
    return Qnil;
  }

  if (argc == 0) {
    rb_raise(rb_eArgError,"mount_to requires at least 1 argument!");
    return Qnil;
  }

  mountpoint = argv[0];

  Check_Type(mountpoint, T_STRING);
  opts = ALLOC(struct fuse_args);
  opts->argc = argc;
  opts->argv = ALLOC_N(char *, opts->argc);
  opts->allocated = 1;
  
  opts->argv[0] = strdup("-odirect_io");

  for (i = 1; i < argc; i++) {
    cur = StringValuePtr(argv[i]);
    opts->argv[i] = ALLOC_N(char, RSTRING_LEN(argv[i]) + 2);
    sprintf(opts->argv[i], "-o%s", cur);
  }

  rb_iv_set(cRbFuse,"@mountpoint",mountpoint);
  fusefs_setup(StringValueCStr(mountpoint), &rf_oper, opts);
  return Qtrue;
}

/* rf_fd
 *
 * Used by: FuseFS.fuse_fd(dir)
 *
 * FuseFS.fuse_fd returns the file descriptor of the open handle on the
 *   /dev/fuse object that is utilized by FUSE. This is crucial for letting
 *   ruby keep control of the script, as it can now use IO.select, rather
 *   than turning control over to fuse_main.
 */
VALUE
rf_fd(VALUE self) {
  int fd = fusefs_fd();
  if (fd < 0)
    return Qnil;
  return INT2NUM(fd);
}

/* rf_process
 *
 * Used for: FuseFS.process
 *
 * rf_process, which calls fusefs_process, is the other crucial portion to
 *   keeping ruby in control of the script. fusefs_process will read and
 *   process exactly one command from the fuse_fd. If this is called when
 *   there is no incoming data waiting, it *will* hang until it receives a
 *   command on the fuse_fd
 */
VALUE
rf_process(VALUE self) {
  fusefs_process();
  return Qnil;
}


/* rf_uid and rf_gid
 *
 * Used by: FuseFS.reader_uid and FuseFS.reader_gid
 *
 * These return the UID and GID of the processes that are causing the
 *   separate Fuse methods to be called. This can be used for permissions
 *   checking, returning a different file for different users, etc.
 */
VALUE
rf_uid(VALUE self) {
  int fd = fusefs_uid();
  if (fd < 0)
    return Qnil;
  return INT2NUM(fd);
}

VALUE
rf_gid(VALUE self) {
  int fd = fusefs_gid();
  if (fd < 0)
    return Qnil;
  return INT2NUM(fd);
}

static VALUE
rf_debugmode(VALUE self){
  if(debugMode){
    return Qtrue;
  }else{
    return Qfalse;
  }
}

static VALUE
rf_debugmode_set(VALUE self,VALUE val){
  debugMode=RTEST(val);
  return val;
}


/* Init_fusefs_lib()
 *
 * Used by: Ruby, to initialize FuseFS.
 *
 * This is just stuff to set up and establish the Ruby module FuseFS and
 *   its methods.
 */
void
Init_rbfuse_lib() {
  init_time = time(NULL);

  /* module FuseFS */
  cRbFuse = rb_define_module("RbFuse");

  /* Our exception */
  cFSException = rb_define_class_under(cRbFuse,"RbFuseException",rb_eStandardError);

  /* def Fuse.run */
  rb_define_singleton_method(cRbFuse,"fuse_fd",     (rbfunc) rf_fd, 0);
  rb_define_singleton_method(cRbFuse,"reader_uid",  (rbfunc) rf_uid, 0);
  rb_define_singleton_method(cRbFuse,"uid",         (rbfunc) rf_uid, 0);
  rb_define_singleton_method(cRbFuse,"reader_gid",  (rbfunc) rf_gid, 0);
  rb_define_singleton_method(cRbFuse,"gid",         (rbfunc) rf_gid, 0);
  rb_define_singleton_method(cRbFuse,"process",     (rbfunc) rf_process, 0);
  rb_define_singleton_method(cRbFuse,"mount_to",    (rbfunc) rf_mount_to, -1);
  rb_define_singleton_method(cRbFuse,"mount_under", (rbfunc) rf_mount_to, -1);
  rb_define_singleton_method(cRbFuse,"mountpoint",  (rbfunc) rf_mount_to, -1);
  rb_define_singleton_method(cRbFuse,"set_root",    (rbfunc) rf_set_root, 1);
  rb_define_singleton_method(cRbFuse,"root=",       (rbfunc) rf_set_root, 1);
  rb_define_singleton_method(cRbFuse,"debug",(rbfunc)rf_debugmode,0);
  rb_define_singleton_method(cRbFuse,"debug=",(rbfunc)rf_debugmode_set,1);
  

  rb_iv_set(cRbFuse,"@handles",rb_hash_new());

  rb_define_const(cRbFuse,"S_IFDIR",INT2FIX(S_IFDIR));
  rb_define_const(cRbFuse,"S_IFREG",INT2FIX(S_IFREG));






}
