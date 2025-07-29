/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef HAVE_LIBZIPARCHIVE

#include "include/libziparchive_wrapper/libziparchive_wrapper.h"
#include <ziparchive/zip_archive.h>
#include <ziparchive/zip_writer.h>

#include <cstdio>
#include <cstring>

static int open_reader(zip_handle handle)
{
	if (handle->reader || handle->writer)
		return 0;

	return OpenArchive(handle->path, (ZipArchiveHandle *)&handle->reader);
}

static int open_writer(zip_handle handle)
{
	if (handle->reader || handle->writer)
		return 0;

	handle->file = fopen(handle->path, "wb");
	if (!handle->file) {
		printf("ERROR: failed to open %s for writing\n", handle->path);
		return 1;
	}
	handle->writer = new ZipWriter(handle->file);

	return 0;
}

zip_handle libziparchive_open(const char *filename)
{
	auto handle = new zip_handle_t;
	handle->reader = NULL;
	handle->writer = NULL;
	handle->file = NULL;
	handle->path = filename;

	return handle;
}

int libziparchive_close(zip_handle handle)
{
	int r = 0;

	if (!handle)
		return r;

	if (handle->reader)
		CloseArchive((ZipArchiveHandle)handle->reader);

	if (handle->writer) {
		auto writer = (ZipWriter *)handle->writer;
		r |= writer->Finish();
		delete writer;
	}

	if (handle->file)
		r |= fclose(handle->file);

	return r;
}

zip_entry libziparchive_alloc_entry() { return (zip_entry)(new ZipEntry64); }

int libziparchive_release_entry(zip_entry entry)
{
	delete (ZipEntry64 *)entry;

	return 0;
}

int libziparchive_start_iteration(zip_handle handle, zip_cookie *cookie)
{
	int r = open_reader(handle);
	if (r)
		return r;
	return StartIteration((ZipArchiveHandle)handle->reader, cookie);
}

void libziparchive_stop_iteration(zip_cookie cookie) { EndIteration(cookie); }

int libziparchive_next(zip_cookie cookie, zip_entry entry, char **name)
{
	std::string entry_name;

	int r = Next(cookie, (ZipEntry64 *)entry, &entry_name);
	*name = strdup(entry_name.c_str());

	return r;
}

int libziparchive_find_entry(zip_handle handle, const char *name, zip_entry entry)
{
	int r = open_reader(handle);
	if (r)
		return r;

	return FindEntry((ZipArchiveHandle)handle->reader, name, (ZipEntry64 *)entry);
}

int32_t libziparchive_get_mtime(zip_entry entry) { return ((ZipEntry64 *)entry)->mod_time; }

int libziparchive_extract_entry(zip_handle handle, zip_entry entry, uint8_t **data,
				size_t *size)
{
	int r = open_reader(handle);
	if (r)
		return r;

	auto reader = (ZipArchiveHandle)handle->reader;
	auto target = (ZipEntry64 *)entry;

	*size = target->uncompressed_length;
	*data = new uint8_t[*size];

	return ExtractToMemory(reader, target, *data, *size);
}

int libziparchive_write_entry(zip_handle handle, const char *name, uint8_t *data, size_t size,
			      int32_t mtime)
{
	int r = open_writer(handle);
	if (r)
		return r;

	auto writer = (ZipWriter *)handle->writer;

	r |= writer->StartEntryWithTime(name, ZipWriter::kCompress | ZipWriter::kAlign32,
					mtime);
	r |= writer->WriteBytes(data, size);
	r |= writer->FinishEntry();

	return r;
}

#endif
