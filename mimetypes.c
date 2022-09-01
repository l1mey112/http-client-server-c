#include "server.h"

const string mimetypes[mimetypes_len] = {
	slit(".aac"), slit("audio/aac"),
	slit(".avi"), slit("video/x-msvideo"),
	slit(".azw"), slit("application/vnd.amazon.ebook"),
	slit(".bin"), slit("application/octet-stream"),
	slit(".bz"), slit("application/x-bzip"),
	slit(".bz2"), slit("application/x-bzip2"),
	slit(".csh"), slit("application/x-csh"),
	slit(".css"), slit("text/css"),
	slit(".csv"), slit("text/csv"),
	slit(".doc"), slit("application/msword"),
	slit(".dll"), slit("application/octet-stream"),
	slit(".eot"), slit("application/vnd.ms-fontobject"),
	slit(".epub"), slit("application/epub+zip"),
	slit(".gif"), slit("image/gif"),
	slit(".htm"), slit("text/html"),
	slit(".html"), slit("text/html"),
	slit(".ico"), slit("image/x-icon"),
	slit(".ics"), slit("text/calendar"),
	slit(".jar"), slit("application/java-archive"),
	slit(".jpeg"), slit("image/jpeg"),
	slit(".jpg"), slit("image/jpeg"),
	slit(".js"), slit("application/javascript"),
	slit(".json"), slit("application/json"),
	slit(".mid"), slit("audio/midi"),
	slit(".midi"), slit("audio/midi"),
	slit(".mp2"), slit("audio/mpeg"),
	slit(".mp3"), slit("audio/mpeg"),
	slit(".mp4"), slit("video/mp4"),
	slit(".mpa"), slit("video/mpeg"),
	slit(".mpe"), slit("video/mpeg"),
	slit(".mpeg"), slit("video/mpeg"),
	slit(".oga"), slit("audio/ogg"),
	slit(".ogv"), slit("video/ogg"),
	slit(".ogx"), slit("application/ogg"),
	slit(".otf"), slit("font/otf"),
	slit(".png"), slit("image/png"),
	slit(".pdf"), slit("application/pdf"),
	slit(".rar"), slit("application/x-rar-compressed"),
	slit(".rtf"), slit("application/rtf"),
	slit(".sh"), slit("application/x-sh"),
	slit(".svg"), slit("image/svg+xml"),
	slit(".swf"), slit("application/x-shockwave-flash"),
	slit(".tar"), slit("application/x-tar"),
	slit(".tif"), slit("image/tiff"),
	slit(".tiff"), slit("image/tiff"),
	slit(".ts"), slit("application/typescript"),
	slit(".ttf"), slit("font/ttf"),
	slit(".txt"), slit("text/plain"),
	slit(".wav"), slit("audio/x-wav"),
	slit(".weba"), slit("audio/webm"),
	slit(".webm"), slit("video/webm"),
	slit(".webp"), slit("image/webp"),
	slit(".woff"), slit("font/woff"),
	slit(".woff2"), slit("font/woff2"),
	slit(".xhtml"), slit("application/xhtml+xml"),
	slit(".xml"), slit("application/xml"),
	slit(".zip"), slit("application/zip"),
	slit(".7z"), slit("application/x-7z-compressed")
};

// static_assert(sizeof(mimetypes) / sizeof(string) % 2 == 0, "Mimetypes list must be even");
// static_assert(sizeof(mimetypes) / sizeof(string) == mimetypes_len, "Update mimetypes length");

#define default_mimetype slit("text/plain")

string match_file_type(string path){
	int last_dot = -1;

	for (int i = 0; i < path.len; i++)
	{
		if (path.cstr[i] == '.') last_dot = i;
	}
	if (last_dot == -1)
		return default_mimetype;
	
	string path_ext = string_substr(path, last_dot, path.len);
	if (path_ext.len == 1)
		return default_mimetype;

	for (int i = 0; i < mimetypes_len; i += 2)
	{
		if (string_equals(path_ext,mimetypes[i]))
			return mimetypes[i+1];
	}
	return default_mimetype;
}