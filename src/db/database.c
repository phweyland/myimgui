//#include <glib.h>
#include "rc.h"
#include "database.h"
#include "murmur3.h"
#include "../gui/gui.h"
#include "../core/log.h"
#include "../core/threads.h"
#include <stdio.h>
#include <stdlib.h>

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
  printf("impdt_work started %s\n", impdt->dt_dir);
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

  sqlite3_exec(ap_db.handle, "INSERT INTO main.folders (id, folder) "
                             "SELECT id, folder FROM library.film_rolls", NULL, NULL, NULL);
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
    sqlite3_prepare_v2(ap_db.handle, "INSERT INTO main.images (id, folder_id, filename, temp) "
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

  sqlite3_exec(ap_db.handle, "INSERT INTO main.tagged_images (imgid, tagid) "
                             "SELECT imgid, tagid FROM library.tagged_images "
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

  sqlite3_exec(db->handle, "CREATE TABLE main.folders (id INTEGER PRIMARY KEY, folder VARCHAR NOT NULL)", NULL, NULL, NULL);
  sqlite3_exec(db->handle, "CREATE INDEX main.folders_folder_index ON folders (folder)", NULL, NULL, NULL);

  sqlite3_exec(db->handle, "CREATE TABLE main.tags (id INTEGER PRIMARY KEY, name VARCHAR, "
                           "synonyms VARCHAR, flags INTEGER)", NULL, NULL, NULL);
  sqlite3_exec(db->handle, "CREATE UNIQUE INDEX data.tags_name_idx ON tags (name)", NULL, NULL, NULL);

  sqlite3_exec(db->handle, "CREATE TABLE main.images (id INTEGER PRIMARY KEY, "
                           "folder_id INTEGER, filename VARCHAR, "
                           "FOREIGN KEY(folder_id) REFERENCES folders(id) ON DELETE CASCADE ON UPDATE CASCADE)",
               NULL, NULL, NULL);
  sqlite3_exec(db->handle, "CREATE INDEX main.images_folder_id_index ON images (folder_id, filename)", NULL, NULL, NULL);

  sqlite3_exec(db->handle, "CREATE TABLE main.tagged_images (imgid INTEGER, tagid INTEGER, "
                           "PRIMARY KEY (imgid, tagid),"
                           "FOREIGN KEY(imgid) REFERENCES images(id) ON UPDATE CASCADE ON DELETE CASCADE)", NULL, NULL, NULL);
  sqlite3_exec(db->handle, "CREATE INDEX main.tagged_images_tagid_index ON tagged_images (tagid)", NULL, NULL, NULL);
}

void ap_init_db()
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
