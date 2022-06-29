//#include <glib.h>
#include "rc.h"
#include "database.h"
#include "murmur3.h"
#include "../gui/gui.h"
#include "../core/log.h"
#include "../core/threads.h"
#include <stdio.h>
#include <stdlib.h>
//#include <glib.h>

ap_db_t ap_db;

typedef struct impdt_job_t
{
  char dt_dir[256];
  char *text;
  int *abort;
  int taskid;
} impdt_job_t;

void impdt_work(uint32_t item, void *arg)
{
  impdt_job_t *impdt = (impdt_job_t *)arg;
  char dt_library[256];
  char dt_data[256];
  snprintf(dt_library, sizeof(dt_library), "%s/library.db", impdt->dt_dir);
  snprintf(dt_data, sizeof(dt_data), "%s/data.db", impdt->dt_dir);

  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(ap_db.handle, "ATTACH DATABASE ?1 AS library", -1, &stmt, NULL);
  sqlite3_bind_text(stmt, 1, dt_library, -1, SQLITE_TRANSIENT);
  if(rc != SQLITE_OK || sqlite3_step(stmt) != SQLITE_DONE)
  {
    const char *err = sqlite3_errmsg(ap_db.handle);
    dt_log(s_log_db|s_log_err, "failed to open the db: %s", err);
    sqlite3_finalize(stmt);
    return;
  }
  sqlite3_finalize(stmt);

  rc = sqlite3_prepare_v2(ap_db.handle, "ATTACH DATABASE ?1 AS data", -1, &stmt, NULL);
  sqlite3_bind_text(stmt, 1, dt_data, -1, SQLITE_TRANSIENT);
  if(rc != SQLITE_OK || sqlite3_step(stmt) != SQLITE_DONE)
  {
    const char *err = sqlite3_errmsg(ap_db.handle);
    dt_log(s_log_db|s_log_err, "failed to open the db: %s", err);
    sqlite3_finalize(stmt);
    return;
  }
  sqlite3_finalize(stmt);

  int number = 0;
  sqlite3_prepare_v2(ap_db.handle, "SELECT COUNT (*) FROM library.images", -1, &stmt, NULL);
  if(sqlite3_step(stmt) == SQLITE_ROW)
    number = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);

  sqlite3_exec(ap_db.handle, "DELETE FROM main.images", NULL, NULL, NULL);
  sqlite3_exec(ap_db.handle, "DELETE FROM main.tags", NULL, NULL, NULL);
  sqlite3_exec(ap_db.handle, "DELETE FROM main.folders", NULL, NULL, NULL);
  sqlite3_exec(ap_db.handle, "DELETE FROM main.tagged_images", NULL, NULL, NULL);

  sqlite3_exec(ap_db.handle, "ALTER TABLE main.images ADD COLUMN temp INTEGER", NULL, NULL, NULL);
  sqlite3_exec(ap_db.handle, "CREATE UNIQUE INDEX main.images_temp_index ON images (temp)", NULL, NULL, NULL);

  sqlite3_exec(ap_db.handle, "INSERT INTO main.folders (id, rootid, folder) "
                             "SELECT f.id, r.id, SUBSTR(f.folder, LENGTH(r.root)+1) "
                             "FROM library.film_rolls AS f "
                             "JOIN main.roots AS r ON r.root = SUBSTR(f.folder, 1, LENGTH(r.root))", NULL, NULL, NULL);

  sqlite3_exec(ap_db.handle, "INSERT INTO main.tags SELECT * FROM data.tags", NULL, NULL, NULL);

  sqlite3_prepare_v2(ap_db.handle, "SELECT id, film_id, filename FROM library.images", -1, &stmt, NULL);
  int count = 0;
  int faulty = 0;
  while(sqlite3_step(stmt) == SQLITE_ROW)
  {
    const char *filename = sqlite3_column_text(stmt, 2);
    const int imgid = sqlite3_column_int(stmt, 0);
    const int hash = murmur_hash3(filename, strlen(filename), 1337);
    sqlite3_stmt *stmt2;
    sqlite3_prepare_v2(ap_db.handle, "INSERT INTO main.images (id, folderid, filename, temp) "
                                     "VALUES (?1, ?2, ?3, ?4)", -1, &stmt2, NULL);
    sqlite3_bind_int(stmt2, 1, imgid);
    sqlite3_bind_int(stmt2, 2, sqlite3_column_int(stmt, 1));
    sqlite3_bind_text(stmt2, 3, filename, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt2, 4, hash);
    if(sqlite3_step(stmt2) != SQLITE_DONE)
      faulty++;
    sqlite3_finalize(stmt2);
    count++;
    snprintf(impdt->text, 256, "%d/%d error(s) %d", count, number, faulty);
    if(*impdt->abort == 1)
      break;
  }
  sqlite3_finalize(stmt);

  sqlite3_exec(ap_db.handle, "INSERT INTO main.tagged_images SELECT imgid, tagid, position "
                             "FROM library.tagged_images "
                             "JOIN main.images ON id = imgid", NULL, NULL, NULL);

  sqlite3_exec(ap_db.handle, "UPDATE main.images SET id = temp", NULL, NULL, NULL);

  sqlite3_exec(ap_db.handle, "DROP INDEX IF EXISTS main.images_temp_index", NULL, NULL, NULL);
  sqlite3_exec(ap_db.handle, "ALTER TABLE main.images DROP COLUMN temp", NULL, NULL, NULL);
  snprintf(impdt->text, 256, "%s", "update done");

  sqlite3_prepare_v2(ap_db.handle, "DETACH DATABASE library", -1, &stmt, NULL);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(ap_db.handle, "DETACH DATABASE data", -1, &stmt, NULL);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

int ap_import_darktable(const char *dt_dir, char *text, int *abort)
{
  static impdt_job_t impdt;
  snprintf(impdt.dt_dir, sizeof(impdt.dt_dir), "%s", dt_dir);
  impdt.text = text;
  impdt.abort = abort;
  impdt.taskid = threads_task(1, -1, &impdt, impdt_work, NULL);
  return impdt.taskid;
}

static void _create_schema(ap_db_t *db)
{

  sqlite3_exec(db->handle, "CREATE TABLE main.roots (id INTEGER PRIMARY KEY, root VARCHAR NOT NULL)", NULL, NULL, NULL);
  sqlite3_exec(db->handle, "CREATE INDEX main.roots_root_index ON roots (root)", NULL, NULL, NULL);

  sqlite3_exec(db->handle, "CREATE TABLE main.folders (id INTEGER PRIMARY KEY, rootid INTEGER, folder VARCHAR NOT NULL,"
                           "FOREIGN KEY(rootid) REFERENCES roots(id) ON DELETE CASCADE ON UPDATE CASCADE)", NULL, NULL, NULL);
  sqlite3_exec(db->handle, "CREATE INDEX main.folders_folder_index ON folders (rootid, folder)", NULL, NULL, NULL);

  sqlite3_exec(db->handle, "CREATE TABLE main.tags (id INTEGER PRIMARY KEY, name VARCHAR, "
                           "synonyms VARCHAR, flags INTEGER)", NULL, NULL, NULL);
  sqlite3_exec(db->handle, "CREATE UNIQUE INDEX main.tags_name_idx ON tags (name)", NULL, NULL, NULL);

  sqlite3_exec(db->handle, "CREATE TABLE main.images (id INTEGER PRIMARY KEY, "
                           "folderid INTEGER, filename VARCHAR, "
                           "FOREIGN KEY(folderid) REFERENCES folders(id) ON DELETE CASCADE ON UPDATE CASCADE)", NULL, NULL, NULL);
  sqlite3_exec(db->handle, "CREATE INDEX main.images_folderid_index ON images (folderid, filename)", NULL, NULL, NULL);

  sqlite3_exec(db->handle, "CREATE TABLE main.tagged_images (imgid INTEGER, tagid INTEGER, position INTEGER, "
                           "PRIMARY KEY (imgid, tagid),"
                           "FOREIGN KEY(imgid) REFERENCES images(id) ON UPDATE CASCADE ON DELETE CASCADE,"
                           "FOREIGN KEY(tagid) REFERENCES tags(id) ON UPDATE CASCADE ON DELETE CASCADE)", NULL, NULL, NULL);
  sqlite3_exec(db->handle, "CREATE INDEX main.tagged_images_tagid_index ON tagged_images (tagid)", NULL, NULL, NULL);
  sqlite3_exec(db->handle, "CREATE INDEX main.tagged_images_position_index ON tagged_images (position)", NULL, NULL, NULL);
}

void ap_db_init()
{
  apdt.db = &ap_db;
//  sqlite3_config(SQLITE_CONFIG_SERIALIZED);
  sqlite3_initialize();
  char dbfile[512];
  snprintf(dbfile, sizeof(dbfile), "%s/.config/%s/db.db", getenv("HOME"), ap_name);
  FILE *file;
  int db_exists = 0;
  if (file = fopen(dbfile, "r"))
  {
    fclose(file);
    db_exists = 1;
  }

  if(sqlite3_open(dbfile, &ap_db.handle))
  {
    const char *err = sqlite3_errmsg(ap_db.handle);
    dt_log(s_log_db|s_log_err, "failed to open the db: %s", err);
    sqlite3_close(ap_db.handle);
    return;
  }

  // some sqlite3 config
//  sqlite3_exec(db->handle, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
//  sqlite3_exec(db->handle, "PRAGMA journal_mode = MEMORY", NULL, NULL, NULL);
//  sqlite3_exec(db->handle, "PRAGMA page_size = 32768", NULL, NULL, NULL);
  sqlite3_exec(ap_db.handle, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);

  if(!db_exists)
  {
    _create_schema(&ap_db);
  }
}

int ap_db_get_subnodes(const char *parent, const int type, ap_nodes_t **nodes)
{
  if(!nodes)
    return 0;
  *nodes = 0;
  sqlite3_stmt *stmt;
  if(type == 0)
  {
    const char sep[4] = "/";
    if(parent[0])
    {
      sqlite3_prepare_v2(ap_db.handle,
                         "SELECT folder, MAX(parent) FROM (SELECT DISTINCT"
                         " CASE WHEN instr(SUBSTR(folder, LENGTH(?1||?2)+1), ?2) > 0"
                         "  THEN substr(folder, LENGTH(?1||?2)+1, instr(SUBSTR(folder, LENGTH(?1||?2)+1), ?2)-1)"
                         "  ELSE substr(folder, LENGTH(?1||?2)+1)  END AS folder,"
                         " CASE WHEN instr(SUBSTR(folder, LENGTH(?1||?2)+1), ?2) > 0 THEN 1 ELSE 0 END as parent "
                         "FROM main.folders "
                         "WHERE SUBSTR(folder, 1, LENGTH(?1||?2)) = ?1||?2) GROUP BY folder",
                         -1, &stmt, NULL);
      sqlite3_bind_text(stmt, 1, parent, -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 2, sep, -1, SQLITE_STATIC);
    }
    else
    {
      sqlite3_prepare_v2(ap_db.handle,
                         "SELECT folder, MAX(parent) FROM (SELECT DISTINCT"
                         " CASE WHEN instr(folder, ?1) > 0"
                         "  THEN substr(folder, 1, instr(folder, ?1)-1)"
                         "  ELSE folder END AS folder,"
                         " CASE WHEN instr(folder, ?1) > 0 THEN 1 ELSE 0 END as parent "
                         "FROM main.folders) GROUP BY folder",
                         -1, &stmt, NULL);
      sqlite3_bind_text(stmt, 1, sep, -1, SQLITE_STATIC);
    }
  }
  else if (type == 1)
  {
    const char sep[4] = "|";
    if(parent[0])
    {
      sqlite3_prepare_v2(ap_db.handle,
                         "SELECT name, MAX(parent) FROM (SELECT DISTINCT"
                         " CASE WHEN instr(SUBSTR(name, LENGTH(?1||?2)+1), ?2) > 0"
                         "  THEN substr(name, LENGTH(?1||?2)+1, instr(SUBSTR(name, LENGTH(?1||?2)+1), ?2)-1)"
                         "  ELSE substr(name, LENGTH(?1||?2)+1)  END AS name,"
                         " CASE WHEN instr(SUBSTR(name, LENGTH(?1||?2)+1), ?2) > 0 THEN 1 ELSE 0 END as parent "
                         "FROM main.tags "
                         "WHERE SUBSTR(name, 1, LENGTH(?1||?2)) = ?1||?2) GROUP BY name",
                         -1, &stmt, NULL);
      sqlite3_bind_text(stmt, 1, parent, -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 2, sep, -1, SQLITE_STATIC);
    }
    else
    {
      sqlite3_prepare_v2(ap_db.handle,
                         "SELECT name, MAX(parent) FROM (SELECT DISTINCT"
                         " CASE WHEN instr(name, ?1) > 0"
                         "  THEN substr(name, 1, instr(name, ?1)-1)"
                         "  ELSE name END AS name,"
                         " CASE WHEN instr(name, ?1) > 0 THEN 1 ELSE 0 END as parent "
                         "FROM main.tags) GROUP BY name",
                         -1, &stmt, NULL);
      sqlite3_bind_text(stmt, 1, sep, -1, SQLITE_STATIC);
    }
  }
  int count = 0;
  while(sqlite3_step(stmt) == SQLITE_ROW)
    count++;
  if(!count)
  {
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_reset(stmt);
  *nodes = calloc(count, sizeof(ap_nodes_t));
  ap_nodes_t *node = *nodes;
  for(int i = 0; i < count && sqlite3_step(stmt) == SQLITE_ROW; i++)
  {
    snprintf(node->name, sizeof(node->name), "%s", sqlite3_column_text(stmt, 0));
    node->parent = sqlite3_column_int(stmt, 1);
    node++;
  }
  sqlite3_finalize(stmt);
  return count;
}

int ap_db_get_images(const char *node, const int type, ap_image_t **images)
{
  if(!images)
    return 0;
  *images = 0;
  sqlite3_stmt *stmt;
  if(type == 0)
  {
    sqlite3_prepare_v2(ap_db.handle,
                       "SELECT i.id, i.filename, r.root || f.folder as path FROM main.images AS i "
                       "JOIN main.folders AS f ON f.id = i.folderid "
                       "JOIN main.roots AS r ON r.id = f.rootid "
                       "WHERE f.folder = ?1",
                       -1, &stmt, NULL);
  }
  else if(type == 1)
  {
    const char sep[4] = "|";
    sqlite3_prepare_v2(ap_db.handle,
                       "SELECT i.id, i.filename, r.root || f.folder as path FROM main.images AS i "
                       "JOIN main.tagged_images AS ti ON ti.imgid = i.id "
                       "JOIN main.tags AS t ON t.id = ti.tagid "
                       "JOIN main.folders AS f ON f.id = i.folderid "
                       "JOIN main.roots AS r ON r.id = f.rootid "
                       "WHERE t.name = ?1",
                       -1, &stmt, NULL);
  }
  sqlite3_bind_text(stmt, 1, node, -1, SQLITE_STATIC);
  int count = 0;

  while(sqlite3_step(stmt) == SQLITE_ROW)
    count++;

  const char *err = sqlite3_errmsg(ap_db.handle);
  if(!count)
  {
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_reset(stmt);
  *images = calloc(count, sizeof(ap_image_t));
  ap_image_t *image = *images;
  for(int i = 0; i < count && sqlite3_step(stmt) == SQLITE_ROW; i++)
  {
    image->imgid = sqlite3_column_int(stmt, 0);
    snprintf(image->filename, sizeof(image->filename), "%s", sqlite3_column_text(stmt, 1));
    snprintf(image->path, sizeof(image->path), "%s", sqlite3_column_text(stmt, 2));
    image++;
  }
  sqlite3_finalize(stmt);
  return count;
}
