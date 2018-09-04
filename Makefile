all: net_raid_client net_raid_server

net_raid_client: client/net_raid_client.c client/net_raid_client.h client/raid1_fuse.c client/raid1_fuse.h client/log.c client/log.h
	gcc -Wall client/log.c client/raid1_fuse.c client/net_raid_client.c -g `pkg-config fuse --cflags --libs` -o net_raid_client


net_raid_server: server/server.c server/server.h
	gcc server/server.c  -lcrypto  -o net_raid_server

clean:
	rm net_raid_client net_raid_server

