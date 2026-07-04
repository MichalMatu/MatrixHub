#include <unity.h>
#include <LittleFS.h>

#include "../../src/filemanager/infrastructure/support/FileManagerPathUtils.cpp"
#include "../../src/filemanager/infrastructure/backends/registry/StorageBackendRegistry.cpp"
#include "../../src/filemanager/infrastructure/backends/registry/StorageBackendRegistryMetrics.cpp"
#include "../../src/filemanager/infrastructure/backends/registry/StorageBackendRegistryPaths.cpp"
#include "../../src/filemanager/infrastructure/backends/service/StorageService.cpp"
#include "../../src/filemanager/infrastructure/backends/resolver/FileManagerBackendResolver.cpp"

#ifdef NATIVE_BUILD
namespace LOG {
void Logging::log(esp_log_level_t, const char*, const char*, ...) {}
}  // namespace LOG

namespace SYSTEM {
bool TaskWatchdog::reset() { return true; }
}  // namespace SYSTEM
#endif

FS LittleFS;

using namespace FILEMGR;

void test_canonicalize_normalizes_duplicate_slashes_and_relative_paths() {
	String canonicalPath;
	TEST_ASSERT_TRUE(FileManagerPathUtils::canonicalizeAbsolutePath(" data//logs/ ", "/", canonicalPath));
	TEST_ASSERT_EQUAL_STRING("/data/logs", canonicalPath.c_str());
}

void test_canonicalize_rejects_parent_traversal() {
	String canonicalPath;
	TEST_ASSERT_FALSE(
		FileManagerPathUtils::canonicalizeAbsolutePath("/littlefs/../config", "/", canonicalPath)
	);
	TEST_ASSERT_EQUAL(0, canonicalPath.length());
}

void test_canonicalize_rejects_backslashes() {
	String canonicalPath;
	TEST_ASSERT_FALSE(FileManagerPathUtils::canonicalizeAbsolutePath("\\config", "/", canonicalPath));
	TEST_ASSERT_EQUAL(0, canonicalPath.length());
}

void test_psram_canonicalize_matches_string_contract() {
	SYSTEM::PsramString canonicalPath;
	const SYSTEM::PsramString path = SYSTEM::makePsramString(" data//logs/ ");
	const SYSTEM::PsramString fallback = SYSTEM::makePsramString("/");

	TEST_ASSERT_TRUE(FileManagerPathUtils::canonicalizeAbsolutePath(path, fallback, canonicalPath));
	TEST_ASSERT_EQUAL_STRING("/data/logs", canonicalPath.c_str());
}

void test_psram_join_paths_rejects_parent_traversal() {
	const SYSTEM::PsramString base = SYSTEM::makePsramString("/safe");
	const SYSTEM::PsramString child = SYSTEM::makePsramString("../config");
	const SYSTEM::PsramString joined = FileManagerPathUtils::joinPaths(base, child);

	TEST_ASSERT_TRUE(joined.empty());
}

void test_sanitize_filename_neutralizes_paths_and_parent_segments() {
	TEST_ASSERT_EQUAL_STRING(
		"__config_secret.json",
		FileManagerPathUtils::sanitizeFilename("../config/secret.json").c_str()
	);
	TEST_ASSERT_EQUAL_STRING(
		"evil_name.bin",
		FileManagerPathUtils::sanitizeFilename("evil\\name.bin").c_str()
	);
	TEST_ASSERT_EQUAL_STRING(
		"upload.bin",
		FileManagerPathUtils::sanitizeFilename("").c_str()
	);
}

void test_registry_translates_littlefs_prefix_after_canonicalization() {
	StorageBackendRegistry registry(&LittleFS);
	TEST_ASSERT_EQUAL_STRING(
		"/data/file.txt",
		registry.toFilesystemPath(SYSTEM::makePsramString("/littlefs//data/file.txt")).c_str()
	);
	TEST_ASSERT_EQUAL_STRING("/", registry.toFilesystemPath(SYSTEM::makePsramString("/littlefs")).c_str());
}

void test_registry_psram_overloads_match_string_contract() {
	StorageBackendRegistry registry(&LittleFS);
	const SYSTEM::PsramString path = SYSTEM::makePsramString("/littlefs//data/file.txt");

	const SYSTEM::PsramString nativePath = registry.toFilesystemPath(path);
	TEST_ASSERT_EQUAL_STRING("/data/file.txt", nativePath.c_str());
	TEST_ASSERT_NOT_NULL(registry.resolveFilesystem(path));
}

void test_registry_rejects_sdcard_backend_without_backend_support() {
	StorageBackendRegistry registry(&LittleFS);
	TEST_ASSERT_NULL(registry.resolveFilesystem(SYSTEM::makePsramString("/sdcard/log.txt")));
	TEST_ASSERT_TRUE(registry.toFilesystemPath(SYSTEM::makePsramString("/sdcard/log.txt")).empty());
}

void test_access_policy_blocks_config_after_backend_translation() {
	StorageBackendRegistry registry(&LittleFS);
	const SYSTEM::PsramString nativePath =
		registry.toFilesystemPath(SYSTEM::makePsramString("/littlefs/config/settings.json"));
	TEST_ASSERT_EQUAL_STRING("/config/settings.json", nativePath.c_str());
	TEST_ASSERT_TRUE(
		FileManagerPathUtils::isAccessAllowed(nativePath, FileManagerPathUtils::FileManagerPathAccess::List)
	);
	TEST_ASSERT_TRUE(
		FileManagerPathUtils::isAccessAllowed(nativePath, FileManagerPathUtils::FileManagerPathAccess::Download)
	);
	TEST_ASSERT_FALSE(
		FileManagerPathUtils::isAccessAllowed(nativePath, FileManagerPathUtils::FileManagerPathAccess::Upload)
	);
	TEST_ASSERT_FALSE(
		FileManagerPathUtils::isAccessAllowed("/config", FileManagerPathUtils::FileManagerPathAccess::Remove)
	);
}

void test_access_policy_blocks_uploads_to_log_storage() {
	TEST_ASSERT_TRUE(
		FileManagerPathUtils::isAccessAllowed("/data-backup", FileManagerPathUtils::FileManagerPathAccess::Upload)
	);
	TEST_ASSERT_FALSE(
		FileManagerPathUtils::isAccessAllowed("/data", FileManagerPathUtils::FileManagerPathAccess::Upload)
	);
	TEST_ASSERT_FALSE(
		FileManagerPathUtils::isAccessAllowed("/data/2026-06", FileManagerPathUtils::FileManagerPathAccess::Upload)
	);
	TEST_ASSERT_FALSE(
		FileManagerPathUtils::isAccessAllowed(
			"/data/2026-06/2026-06-18.bin",
			FileManagerPathUtils::FileManagerPathAccess::Upload
		)
	);
	TEST_ASSERT_TRUE(
		FileManagerPathUtils::isAccessAllowed(
			"/data/2026-06/2026-06-18.bin",
			FileManagerPathUtils::FileManagerPathAccess::Download
		)
	);
	TEST_ASSERT_TRUE(
		FileManagerPathUtils::isAccessAllowed(
			"/data/2026-06/old.bin",
			FileManagerPathUtils::FileManagerPathAccess::Remove
		)
	);
}

void test_storage_service_psram_translation_matches_registry() {
	StorageService storage(&LittleFS);
	const SYSTEM::PsramString nativePath =
		storage.toFilesystemPath(SYSTEM::makePsramString("/littlefs/config/settings.json"));

	TEST_ASSERT_EQUAL_STRING("/config/settings.json", nativePath.c_str());
}

void test_resolver_rejects_invalid_upload_directory_after_normalization() {
	StorageService storage(&LittleFS);
	FileManagerBackendResolver resolver(&storage);

	const FileManagerBackendResolution resolution =
		resolver.resolveForUpload(SYSTEM::makePsramString("/littlefs/../config"));

	TEST_ASSERT_NULL(resolution.filesystem);
	TEST_ASSERT_TRUE(resolution.nativePath.empty());
}

#ifndef NATIVE_BUILD
void setup() {
	UNITY_BEGIN();
	RUN_TEST(test_canonicalize_normalizes_duplicate_slashes_and_relative_paths);
	RUN_TEST(test_canonicalize_rejects_parent_traversal);
	RUN_TEST(test_canonicalize_rejects_backslashes);
	RUN_TEST(test_psram_canonicalize_matches_string_contract);
	RUN_TEST(test_psram_join_paths_rejects_parent_traversal);
	RUN_TEST(test_sanitize_filename_neutralizes_paths_and_parent_segments);
	RUN_TEST(test_registry_translates_littlefs_prefix_after_canonicalization);
	RUN_TEST(test_registry_psram_overloads_match_string_contract);
	RUN_TEST(test_registry_rejects_sdcard_backend_without_backend_support);
	RUN_TEST(test_access_policy_blocks_config_after_backend_translation);
	RUN_TEST(test_access_policy_blocks_uploads_to_log_storage);
	RUN_TEST(test_storage_service_psram_translation_matches_registry);
	RUN_TEST(test_resolver_rejects_invalid_upload_directory_after_normalization);
	UNITY_END();
}

void loop() {}
#else
int main(int argc, char** argv) {
	(void)argc;
	(void)argv;
	UNITY_BEGIN();
	RUN_TEST(test_canonicalize_normalizes_duplicate_slashes_and_relative_paths);
	RUN_TEST(test_canonicalize_rejects_parent_traversal);
	RUN_TEST(test_canonicalize_rejects_backslashes);
	RUN_TEST(test_psram_canonicalize_matches_string_contract);
	RUN_TEST(test_psram_join_paths_rejects_parent_traversal);
	RUN_TEST(test_sanitize_filename_neutralizes_paths_and_parent_segments);
	RUN_TEST(test_registry_translates_littlefs_prefix_after_canonicalization);
	RUN_TEST(test_registry_psram_overloads_match_string_contract);
	RUN_TEST(test_registry_rejects_sdcard_backend_without_backend_support);
	RUN_TEST(test_access_policy_blocks_config_after_backend_translation);
	RUN_TEST(test_access_policy_blocks_uploads_to_log_storage);
	RUN_TEST(test_storage_service_psram_translation_matches_registry);
	RUN_TEST(test_resolver_rejects_invalid_upload_directory_after_normalization);
	return UNITY_END();
}
#endif
