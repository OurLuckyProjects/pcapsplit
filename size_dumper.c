//  Copyright (C) 2008 Lothar Braun <lothar@lobraun.de>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "size_dumper.h"

#include <stdlib.h>
#include <string.h>

#define MAX_FILENAME 65535

struct dumping_module size_dumper = {
	size_dumper_init,
	size_dumper_finish,
	size_dumper_run
};


struct size_dumper_data {
	char base_filename[MAX_FILENAME];
	char dump_filename[MAX_FILENAME];
	size_t number;
	size_t file_data_count;
	size_t max_file_data_count;
	pcap_t* out;
	pcap_dumper_t* dumper;
};

void createNewFile(struct size_dumper_data* data)
{
	snprintf(data->dump_filename, MAX_FILENAME, "%s.%lu",
		data->base_filename, data->number);
	//TODO: use DLT from source file
	data->out = pcap_open_dead(DLT_EN10MB, 65535);
	data->dumper = pcap_dump_open(data->out, data->dump_filename);
	data->number++;
	data->file_data_count = 0;
}

int size_dumper_init(struct dumping_module* m, void* data)
{
	struct size_dumper_data* sdata = (struct size_dumper_data*)malloc(
		sizeof(struct size_dumper_data));
	strncpy(sdata->base_filename, (char*)data, MAX_FILENAME);
	sdata->number = 0;
	sdata->file_data_count = 0;
	// TODO: make configurable
	sdata->max_file_data_count = 1000;
	createNewFile(sdata);

	m->module_data = (void*)sdata;

	return 0;
}

int size_dumper_finish(struct dumping_module* m)
{
	struct size_dumper_data* d = (struct size_dumper_data*)m->module_data;
	pcap_dump_flush(d->dumper);
	pcap_dump_close(d->dumper);
	free(m->module_data);
	m->module_data = NULL;
	return 0;
}

int size_dumper_run(struct dumping_module* m, struct packet* p)
{
	struct size_dumper_data* d = (struct size_dumper_data*)m->module_data;
	pcap_dump((unsigned char*)d->dumper, &p->header, p->data);
	d->file_data_count += p->header.len;
	if (d->file_data_count >= d->max_file_data_count) {
		pcap_dump_flush(d->dumper);
		pcap_dump_close(d->dumper);
		createNewFile(d);
	}
	return 0;
}