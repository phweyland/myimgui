#include "core/fs.h"
#include "core/log.h"
#include "db/db.h"
#include "db/murmur3.h"
#include "db/exif.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

int // returns zero on success
fs_copy(
    const char *dst,
    const char *src)
{
  ssize_t ret;
  struct stat sb;
  loff_t len = -1;
  int fd0 = open(src, O_RDONLY), fd1 = -1;
  if(fd0 == -1 || fstat(fd0, &sb) == -1) goto copy_error;
  len = sb.st_size;
  if(sb.st_mode & S_IFDIR) { len = 0; goto copy_error; } // don't copy directories
  if(-1 == (fd1 = open(dst, O_CREAT | O_WRONLY | O_TRUNC, 0644))) goto copy_error;
  do {
    ret = copy_file_range(fd0, 0, fd1, 0, len, 0);
  } while((len-=ret) > 0 && ret > 0);
copy_error:
  if(fd0 >= 0) close(fd0);
  if(fd0 >= 0) close(fd1);
  return len;
}

int // returns zero on success
fs_delete(
    const char *fn)
{
  return unlink(fn);
}

int // returns zero on success
fs_mkdir(
    const char *pathname,
    mode_t      mode)
{
  return mkdir(pathname, mode);
}

void  // returns the directory where the actual binary (not the symlink) resides
fs_basedir(
    char *basedir,  // output will be copied here
    size_t maxlen)  // allocation size
{
#ifdef __linux__
  realpath("/proc/self/exe", basedir);
#elif defined(__FreeBSD__)
  int mib_procpath[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
  size_t len_procpath = maxlen;
  sysctl(mib_procpath, 4, basedir, &len_procpath, NULL, 0);
#else
#error port me
#endif
  char *c = 0;
  for(int i=0;basedir[i]!=0;i++) if(basedir[i] == '/') c = basedir+i;
  if(c) *c = 0; // get dirname, i.e. strip off executable name
}

int // return the number of devices found
fs_find_usb_block_devices(
    char devname[20][20],
    int  mounted[20])
{ // pretty much ls /sys/class/scsi_disk/*/device/block/{sda,sdb,sdd,..}/{..sdd1..} and then grep for it in /proc/mounts
  int cnt = 0;
  char block[1000];
  struct dirent **ent, **ent2;
  int ent_cnt = scandir("/sys/class/scsi_disk/", &ent, 0, alphasort);
  if(ent_cnt < 0) return 0;
  for(int i=0;i<ent_cnt&&cnt<20;i++)
  {
    snprintf(block, sizeof(block), "/sys/class/scsi_disk/%s/device/block", ent[i]->d_name);
    int ent2_cnt = scandir(block, &ent2, 0, alphasort);
    if(ent2_cnt < 0) goto next;
    for(int j=0;j<ent2_cnt&&cnt<20;j++)
    {
      for(int k=1;k<10&&cnt<20;k++)
      {
        snprintf(block, sizeof(block), "/sys/class/scsi_disk/%s/device/block/%s/%.13s%d", ent[i]->d_name,
            ent2[j]->d_name, ent2[j]->d_name, k);
        struct stat sb;
        if(stat(block, &sb)) break;
        if(sb.st_mode & S_IFDIR)
          snprintf(devname[cnt++], sizeof(devname[0]), "/dev/%.13s%d", ent2[j]->d_name, k%10u);
      }
      free(ent2[j]);
    }
    free(ent2);
next:
    free(ent[i]);
  }
  free(ent);
  for(int i=0;i<cnt;i++) mounted[i] = 0;
  FILE *f = fopen("/proc/mounts", "r");
  if(f) while(!feof(f))
  {
    fscanf(f, "%999s %*[^\n]", block);
    for(int i=0;i<cnt;i++) if(!strcmp(block, devname[i])) mounted[i] = 1;
  }
  if(f) fclose(f);
  return cnt;
}

int dt_fs_filter_dir(const struct dirent *d)
{
  if(d->d_name[0] == '.' && d->d_name[1] == '\0') return 0;
  if(d->d_type != DT_DIR) return 0; // filter out non-dirs too
  return 1;
}

int dt_fs_filter_file(const struct dirent *d)
{
  const char *f = d->d_name;
  if(f[0] == '.' && f[1] != '.') return 0;
  if(d->d_type == DT_DIR) return 0;

  const char *f2 = f + strlen(f);
  while(f2 > f && *f2 != '.') f2--;
  return !strcasecmp(f2, ".cr2") ||
         !strcasecmp(f2, ".cr3") ||
         !strcasecmp(f2, ".crw") ||
         !strcasecmp(f2, ".nef") ||
         !strcasecmp(f2, ".pef") ||
         !strcasecmp(f2, ".raw") ||
         !strcasecmp(f2, ".tif") ||
         !strcasecmp(f2, ".orf") ||
         !strcasecmp(f2, ".arw") ||
         !strcasecmp(f2, ".srw") ||
         !strcasecmp(f2, ".nrw") ||
         !strcasecmp(f2, ".kc2") ||
         !strcasecmp(f2, ".dng") ||
         !strcasecmp(f2, ".raf") ||
         !strcasecmp(f2, ".rw2") ||
         !strcasecmp(f2, ".jpg") ||
         !strcasecmp(f2, ".jpeg");
}

int ap_fs_get_subdirs(const char *parent, ap_nodes_t **nodes)
{
  struct dirent **ent = NULL;  // cached directory entries
  int count = scandir(parent, &ent, &dt_fs_filter_dir, alphasort);
  if(count == -1u || count == 0)
    return 0;
  *nodes = (ap_nodes_t *)calloc(count, sizeof(ap_nodes_t));
  ap_nodes_t *node = *nodes;
  for(int i = 0; i < count; i++)
  {
    snprintf(node->name, sizeof(node->name), "%s", ent[i]->d_name);
    node++;
  }
  for(int i = 0; i < count; i++)
    free(ent[i]);
  free(ent);
  return count;
}

int ap_fs_get_images(const char *node, const int type, ap_image_t **images)
{
  if(!images)
    return 0;
  clock_t beg = clock();
  *images = 0;

  struct dirent **ent = NULL;  // cached directory entries
  int count = scandir(node, &ent, &dt_fs_filter_file, alphasort);

  if(count == -1u || count == 0)
    return 0;

  *images = calloc(count, sizeof(ap_image_t));
  ap_image_t *image = *images;
  for(int i = 0; i < count; i++)
  {
    image->imgid = -1u;
    snprintf(image->filename, sizeof(image->filename), "%s", ent[i]->d_name);
    snprintf(image->path, sizeof(image->path), "%s", node);
    if(image->path[strlen(image->path) - 1] == '/')
      image->path[strlen(image->path) - 1] = '\0';
    char fullname[1024];
    snprintf(fullname, sizeof(fullname), "%s/%s.cfg", image->path, ent[i]->d_name);
    image->hash = murmur_hash3(fullname, strnlen(fullname, sizeof(fullname)), 1337);
    image->labels = 0;
    image->rating = 0;
    char model[20];
    char datetime[20];
    fullname[strlen(fullname) - 4] = '\0';
    dt_db_exif_mini(fullname, datetime, model, sizeof(model));
    snprintf(image->datetime, sizeof(image->datetime), "%s.000", datetime);
    image->longitude = NAN;
    image->latitude = NAN;
    image->altitude = NAN;
    image++;
  }

  for(int i = 0; i < count; i++)
    free(ent[i]);
  free(ent);

  clock_t end = clock();
  dt_log(s_log_perf, "[db] ran get dir images in %3.0fms", 1000.0*(end-beg)/CLOCKS_PER_SEC);
  return count;
}
