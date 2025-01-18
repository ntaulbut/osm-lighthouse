CREATE TABLE "changeset_tags" (
	"changeset"	INTEGER,
	"k"	TEXT,
	"v"	TEXT NOT NULL,
	PRIMARY KEY("changeset","k")
);

CREATE TABLE "changesets" (
	"id"	     INTEGER,
	"created_at" TEXT NOT NULL,
	"closed_at"  TEXT,
	"open"	     INTEGER NOT NULL,
	"user"	     TEXT NOT NULL,
	"uid"	     INTEGER NOT NULL,
	"min_lat"    REAL NOT NULL,
	"max_lat"    REAL NOT NULL,
	"min_lon"    REAL NOT NULL,
	"max_lon"    REAL NOT NULL,
	"comments"   INTEGER NOT NULL,
	PRIMARY KEY("id")
);

CREATE TABLE "nodes" (
	"id"	    INTEGER,
	"version"   INTEGER,
	"changeset" INTEGER NOT NULL,
	"action"    TEXT NOT NULL,
	PRIMARY KEY("version","id")
);

CREATE TABLE "relations" (
	"id"	    INTEGER,
	"version"   INTEGER,
	"changeset" INTEGER NOT NULL,
	"action"    TEXT NOT NULL,
	PRIMARY KEY("version","id")
);

CREATE TABLE "ways" (
	"id"	    INTEGER,
	"version"   INTEGER,
	"changeset" INTEGER NOT NULL,
	"action"    TEXT NOT NULL,
	PRIMARY KEY("version","id")
);
