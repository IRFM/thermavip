/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, L�o Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "VipLibRIR.h"
#include "VipCore.h"
#include "VipLogging.h"
#include "VipNetwork.h"

#include <qapplication.h>
#include <qfileinfo.h>

static QLibrary _old_librir;
static QLibrary _tools;
static QLibrary _geometry;
static QLibrary _signal_processing;
static QLibrary _video_io;
static QLibrary _west;

static bool loadLibraries()
{
	static bool init = false;
	static bool res = false;
	if (!init) {
		init = true;
		// Try first to load new librir version (inside librir directory)
		QString path = vipAppCanonicalPath();
		path = QFileInfo(path).canonicalPath();
		path.replace("\\", "/");
		if (!path.endsWith("/"))
			path += "/";
		QString thermavip = path;
		path += "librir/libs/";

		QString path_west = thermavip + "librir_west/libs/";

#ifdef WIN32
		QString tools = path + "tools.dll";
		QString geometry = path + "geometry.dll";
		QString signal_processing = path + "signal_processing.dll";
		QString video_io = path + "video_io.dll";
		QString west = path_west + "west.dll";
#else
		QString tools = path + "tools.so";
		QString geometry = path + "geometry.so";
		QString signal_processing = path + "signal_processing.so";
		QString video_io = path + "video_io.so";
		QString west = path + "west.so";
#endif
		_tools.setFileName(tools);
		_geometry.setFileName(geometry);
		_signal_processing.setFileName(signal_processing);
		_video_io.setFileName(video_io);
		_west.setFileName(west);
		if (!_tools.load() || !_geometry.load() || !_signal_processing.load() || !_video_io.load()) {
			vip_debug("Error loading new librir: %s, fallback to old version\n", _tools.errorString().toLatin1().data());
			fflush(stdout);
			vip_debug("Error loading new librir: %s, fallback to old version\n", _geometry.errorString().toLatin1().data());
			fflush(stdout);
			vip_debug("Error loading new librir: %s, fallback to old version\n", _signal_processing.errorString().toLatin1().data());
			fflush(stdout);
			vip_debug("Error loading new librir: %s, fallback to old version\n", _video_io.errorString().toLatin1().data());
			fflush(stdout);
			vip_debug("Error loading new librir: %s, fallback to old version\n", _west.errorString().toLatin1().data());
			fflush(stdout);
		}
		else {
			// TODO: set TSLIB_SERVER to deneb-bis
			if (true) { // vipPing("deneb-bis")) {
				    // VIP_LOG_INFO("Use ARCADE server deneb-bis");
				    // qputenv("TSLIB_SERVER", "deneb-bis");
				    // qputenv("TSLIB_COMPRESS", "1");
			}
			else
				VIP_LOG_WARNING("Cannot reach server deneb-bis");
			if (!_west.load()) {
				VIP_LOG_WARNING("West plugin of librir not found!");
			}
			return res = true;
		}

		// Load old librir
		_old_librir.setFileName("librir64");
		// bool e = QFileInfo("librir64.dll").exists();
		if (!_old_librir.load()) {
			/*#ifdef __unix__
						QString path = QFileInfo(qApp->arguments()[0]).canonicalPath();
						path += "/librir.so";
						vip_debug("%s\n",path.toLatin1().data());
						librir.library.setFileName(path);
			#else*/
			_old_librir.setFileName("librir");
			// librir.library.setFileName("/Home/VM213788/Thermavip_git/Thermavip/bin/debug/librir.so");
			vip_debug("load '%s'\n", _old_librir.fileName().toLatin1().data());
			fflush(stdout);
			// #endif
			if (!_old_librir.load()) {
				vip_debug("Cannot find librir on this computer: %s\n", _old_librir.errorString().toLatin1().data());
				fflush(stdout);
				VIP_LOG_ERROR("Cannot find librir64 on this computer");
				_old_librir.unload();
				return res = false;
			}
			else {
				vip_debug("success\n");
				fflush(stdout);
			}
		}
		return res = true;
	}
	return res;
}

static QLibrary* toolsLib()
{
	if (_old_librir.isLoaded())
		return &_old_librir;
	else if (_tools.isLoaded())
		return &_tools;
	return nullptr;
}
static QLibrary* geometrLib()
{
	if (_old_librir.isLoaded())
		return &_old_librir;
	else if (_geometry.isLoaded())
		return &_geometry;
	return nullptr;
}
static QLibrary* signalProcessingLib()
{
	if (_old_librir.isLoaded())
		return &_old_librir;
	else if (_signal_processing.isLoaded())
		return &_signal_processing;
	return nullptr;
}
static QLibrary* videoIOLib()
{
	if (_old_librir.isLoaded())
		return &_old_librir;
	else if (_video_io.isLoaded())
		return &_video_io;
	return nullptr;
}
static QLibrary* westLib()
{
	if (_old_librir.isLoaded())
		return &_old_librir;
	else if (_west.isLoaded())
		return &_west;
	return nullptr;
}

VipLibRIR::VipLibRIR() {}

VipLibRIR::~VipLibRIR()
{
	// Unload the top level library (that will unload all the other)
	if (westLib())
		westLib()->unload();
}

VipLibRIR* VipLibRIR::instance()
{
	static VipLibRIR* librir = nullptr;
	if (librir)
		return librir;
	if (!loadLibraries())
		return nullptr;

	librir = new VipLibRIR();

	librir->set_print_function = (_set_print_function)toolsLib()->resolve("set_print_function");
	if (!librir->set_print_function) {
		VIP_LOG_ERROR("librir: missing set_print_function");
		delete librir;
		return librir = nullptr;
	}
	librir->disable_print = (_disable_print)toolsLib()->resolve("disable_print");
	if (!librir->disable_print) {
		VIP_LOG_ERROR("librir: missing disable_print");
		delete librir;
		return librir = nullptr;
	}
	librir->reset_print_functions = (_reset_print_functions)toolsLib()->resolve("reset_print_functions");
	if (!librir->reset_print_functions) {
		VIP_LOG_ERROR("librir: missing reset_print_functions");
		delete librir;
		return librir = nullptr;
	}
	librir->get_last_log_error = (_get_last_log_error)toolsLib()->resolve("get_last_log_error");
	if (!librir->get_last_log_error) {
		VIP_LOG_ERROR("librir: missing get_last_log_error");
		delete librir;
		return librir = nullptr;
	}

	librir->open_camera_file = (_open_camera_file)videoIOLib()->resolve("open_camera_file");
	if (!librir->open_camera_file) {
		VIP_LOG_ERROR("librir: missing open_camera_file");
		delete librir;
		return librir = nullptr;
	}

	librir->set_global_emissivity = (_set_global_emissivity)videoIOLib()->resolve("set_global_emissivity");
	if (!librir->set_global_emissivity) {
		VIP_LOG_ERROR("librir: missing set_global_emissivity");
		delete librir;
		return librir = nullptr;
	}
	librir->set_emissivity = (_set_emissivity)videoIOLib()->resolve("set_emissivity");
	if (!librir->set_emissivity) {
		VIP_LOG_ERROR("librir: missing set_emissivity");
		delete librir;
		return librir = nullptr;
	}
	librir->support_emissivity = (_support_emissivity)videoIOLib()->resolve("support_emissivity");
	if (!librir->support_emissivity) {
		VIP_LOG_ERROR("librir: missing support_emissivity");
		delete librir;
		return librir = nullptr;
	}
	librir->get_emissivity = (_get_emissivity)videoIOLib()->resolve("get_emissivity");
	if (!librir->get_emissivity) {
		VIP_LOG_ERROR("librir: missing get_emissivity");
		delete librir;
		return librir = nullptr;
	}

	librir->load_motion_correction_file = (_load_motion_correction_file)videoIOLib()->resolve("load_motion_correction_file");
	librir->enable_motion_correction = (_enable_motion_correction)videoIOLib()->resolve("enable_motion_correction");
	librir->motion_correction_enabled = (_motion_correction_enabled)videoIOLib()->resolve("motion_correction_enabled");

	librir->get_image_count = (_get_image_count)videoIOLib()->resolve("get_image_count");
	if (!librir->get_image_count) {
		VIP_LOG_ERROR("librir: missing get_image_count");
		delete librir;
		return librir = nullptr;
	}
	librir->get_image_time = (_get_image_time)videoIOLib()->resolve("get_image_time");
	if (!librir->get_image_time) {
		VIP_LOG_ERROR("librir: missing get_image_time");
		delete librir;
		return librir = nullptr;
	}
	librir->get_image_size = (_get_image_size)videoIOLib()->resolve("get_image_size");
	if (!librir->get_image_size) {
		VIP_LOG_ERROR("librir: missing get_image_size");
		delete librir;
		return librir = nullptr;
	}
	librir->supported_calibrations = (_supported_calibrations)videoIOLib()->resolve("supported_calibrations");
	if (!librir->supported_calibrations) {
		VIP_LOG_ERROR("librir: missing supported_calibrations");
		delete librir;
		return librir = nullptr;
	}
	librir->calibration_name = (_calibration_name)videoIOLib()->resolve("calibration_name");
	if (!librir->calibration_name) {
		VIP_LOG_ERROR("librir: missing calibration_name");
		delete librir;
		return librir = nullptr;
	}
	librir->load_image = (_load_image)videoIOLib()->resolve("load_image");
	if (!librir->load_image) {
		VIP_LOG_ERROR("librir: missing load_image");
		delete librir;
		return librir = nullptr;
	}

	librir->get_last_image_raw_value = (_get_last_image_raw_value)videoIOLib()->resolve("get_last_image_raw_value");
	if (!librir->get_last_image_raw_value) {
		VIP_LOG_ERROR("librir: missing get_last_image_raw_value");
		delete librir;
		return librir = nullptr;
	}
	librir->close_camera = (_close_camera)videoIOLib()->resolve("close_camera");
	if (!librir->close_camera) {
		VIP_LOG_ERROR("librir: missing close_camera");
		delete librir;
		return librir = nullptr;
	}
	librir->get_filename = (_get_filename)videoIOLib()->resolve("get_filename");
	if (!librir->get_filename) {
		VIP_LOG_ERROR("librir: missing get_filename");
		delete librir;
		return librir = nullptr;
	}

	librir->camera_saturate = (_camera_saturate)videoIOLib()->resolve("camera_saturate");
	if (!librir->camera_saturate) {
		VIP_LOG_ERROR("librir: missing camera_saturate");
		delete librir;
		return librir = nullptr;
	}

	librir->calibration_files = (_calibration_files)videoIOLib()->resolve("calibration_files");
	if (!librir->calibration_files) {
		VIP_LOG_ERROR("librir: missing camera_saturate");
		delete librir;
		return librir = nullptr;
	}

	librir->get_attribute_count = (_get_attribute_count)videoIOLib()->resolve("get_attribute_count");
	if (!librir->get_attribute_count) {
		VIP_LOG_ERROR("librir: missing get_attribute_count");
		delete librir;
		return librir = nullptr;
	}
	librir->get_attribute = (_get_attribute)videoIOLib()->resolve("get_attribute");
	if (!librir->get_attribute) {
		VIP_LOG_ERROR("librir: missing get_attribute");
		delete librir;
		return librir = nullptr;
	}
	librir->get_global_attribute_count = (_get_attribute_count)videoIOLib()->resolve("get_global_attribute_count");
	if (!librir->get_global_attribute_count) {
		VIP_LOG_ERROR("librir: missing get_global_attribute_count");
		delete librir;
		return librir = nullptr;
	}
	librir->get_global_attribute = (_get_attribute)videoIOLib()->resolve("get_global_attribute");
	if (!librir->get_global_attribute) {
		VIP_LOG_ERROR("librir: missing get_global_attribute");
		delete librir;
		return librir = nullptr;
	}

	librir->enable_bad_pixels = (_enable_bad_pixels)videoIOLib()->resolve("enable_bad_pixels");
	if (!librir->enable_bad_pixels) {
		VIP_LOG_ERROR("librir: missing enable_bad_pixels");
		delete librir;
		return librir = nullptr;
	}
	librir->bad_pixels_enabled = (_bad_pixels_enabled)videoIOLib()->resolve("bad_pixels_enabled");
	if (!librir->bad_pixels_enabled) {
		VIP_LOG_ERROR("librir: missing bad_pixels_enabled");
		delete librir;
		return librir = nullptr;
	}

	/* librir->open_video_write = (_open_video_write)videoIOLib()->resolve("open_video_write");
	if (!librir->open_video_write) {
		VIP_LOG_ERROR("librir: missing open_video_write");
		delete librir;
		return librir = nullptr;
	}
	librir->image_write = (_image_write)videoIOLib()->resolve("image_write");
	if (!librir->image_write) {
		VIP_LOG_ERROR("librir: missing image_write");
		delete librir;
		return librir = nullptr;
	}
	librir->close_video = (_close_video)videoIOLib()->resolve("close_video");
	if (!librir->close_video) {
		VIP_LOG_ERROR("librir: missing close_video");
		delete librir;
		return librir = nullptr;
	}*/

	librir->calibrate_image = (_calibrate_image)videoIOLib()->resolve("calibrate_image");
	if (!librir->calibrate_image) {
		VIP_LOG_ERROR("librir: missing calibrate_image");
		delete librir;
		return librir = nullptr;
	}
	librir->calibrate_image_inplace = (_calibrate_image_inplace)videoIOLib()->resolve("calibrate_image_inplace");
	if (!librir->calibrate_image_inplace) {
		VIP_LOG_ERROR("librir: missing calibrate_image_inplace");
		delete librir;
		return librir = nullptr;
	}

	librir->zstd_compress_bound = (_zstd_compress_bound)toolsLib()->resolve("zstd_compress_bound");
	if (!librir->zstd_compress_bound) {
		VIP_LOG_ERROR("librir: missing zstd_compress_bound");
		delete librir;
		return librir = nullptr;
	}
	librir->zstd_decompress_bound = (_zstd_decompress_bound)toolsLib()->resolve("zstd_decompress_bound");
	if (!librir->zstd_decompress_bound) {
		VIP_LOG_ERROR("librir: missing zstd_decompress_bound");
		delete librir;
		return librir = nullptr;
	}
	librir->zstd_compress = (_zstd_compress)toolsLib()->resolve("zstd_compress");
	if (!librir->zstd_compress) {
		VIP_LOG_ERROR("librir: missing zstd_compress");

		delete librir;
		return librir = nullptr;
	}
	librir->zstd_decompress = (_zstd_decompress)toolsLib()->resolve("zstd_decompress");
	if (!librir->zstd_decompress) {
		VIP_LOG_ERROR("librir: missing zstd_decompress");

		delete librir;
		return librir = nullptr;
	}

	librir->h264_open_file = (_h264_open_file)videoIOLib()->resolve("h264_open_file");
	if (!librir->h264_open_file) {
		VIP_LOG_ERROR("librir: missing h264_open_file");
		delete librir;
		return librir = nullptr;
	}
	librir->h264_close_file = (_h264_close_file)videoIOLib()->resolve("h264_close_file");
	if (!librir->h264_close_file) {
		VIP_LOG_ERROR("librir: missing h264_close_file");
		delete librir;
		return librir = nullptr;
	}
	librir->h264_set_parameter = (_h264_set_parameter)videoIOLib()->resolve("h264_set_parameter");
	if (!librir->h264_set_parameter) {
		VIP_LOG_ERROR("librir: missing h264_set_parameter");
		delete librir;
		return librir = nullptr;
	}
	librir->h264_set_global_attributes = (_h264_set_global_attributes)videoIOLib()->resolve("h264_set_global_attributes");
	if (!librir->h264_set_global_attributes) {
		VIP_LOG_ERROR("librir: missing h264_set_global_attributes");
		delete librir;
		return librir = nullptr;
	}
	librir->h264_add_image_lossless = (_h264_add_image_lossless)videoIOLib()->resolve("h264_add_image_lossless");
	if (!librir->h264_add_image_lossless) {
		VIP_LOG_ERROR("librir: missing h264_add_image_lossless");
		delete librir;
		return librir = nullptr;
	}
	librir->h264_add_image_lossy = (_h264_add_image_lossy)videoIOLib()->resolve("h264_add_image_lossy");
	if (!librir->h264_add_image_lossy) {
		VIP_LOG_ERROR("librir: missing h264_add_image_lossy");
		delete librir;
		return librir = nullptr;
	}
	librir->get_table_names = (_get_table_names)videoIOLib()->resolve("get_table_names");
	if (!librir->get_table_names) {
		VIP_LOG_ERROR("librir: missing get_table_names");
		delete librir;
		return librir = nullptr;
	}
	librir->get_table = (_get_table)videoIOLib()->resolve("get_table");
	if (!librir->get_table) {
		VIP_LOG_ERROR("librir: missing get_table");
		delete librir;
		return librir = nullptr;
	}

	librir->unzip = (_unzip)toolsLib()->resolve("unzip");
	if (!librir->unzip) {
		VIP_LOG_ERROR("librir: missing unzip");
		delete librir;
		return librir = nullptr;
	}

	if (westLib()) {

		librir->set_optical_temperature = (_set_optical_temperature)westLib()->resolve("set_optical_temperature");
		if (!librir->set_optical_temperature) {
			VIP_LOG_ERROR("librir: missing set_optical_temperature");
			delete librir;
			return librir = nullptr;
		}
		librir->get_optical_temperature = (_get_optical_temperature)westLib()->resolve("get_optical_temperature");
		if (!librir->get_optical_temperature) {
			VIP_LOG_ERROR("librir: missing get_optical_temperature");
			delete librir;
			return librir = nullptr;
		}

		librir->set_STEFI_temperature = (_set_optical_temperature)westLib()->resolve("set_STEFI_temperature");
		if (!librir->set_STEFI_temperature) {
			VIP_LOG_ERROR("librir: missing set_STEFI_temperature");
			delete librir;
			return librir = nullptr;
		}
		librir->get_STEFI_temperature = (_get_optical_temperature)westLib()->resolve("get_STEFI_temperature");
		if (!librir->get_STEFI_temperature) {
			VIP_LOG_ERROR("librir: missing get_STEFI_temperature");
			delete librir;
			return librir = nullptr;
		}

		librir->support_optical_temperature = (_support_optical_temperature)westLib()->resolve("support_optical_temperature");
		if (!librir->support_optical_temperature) {
			VIP_LOG_ERROR("librir: missing support_optical_temperature");
			delete librir;
			return librir = nullptr;
		}

		librir->get_temp_directory = (_get_temp_directory)westLib()->resolve("get_west_data_dir");
		if (!librir->get_temp_directory) {
			VIP_LOG_ERROR("librir: missing get_west_data_dir");
			delete librir;
			return librir = nullptr;
		}

		librir->get_default_temp_directory = (_get_temp_directory)westLib()->resolve("get_default_west_data_dir");
		if (!librir->get_default_temp_directory) {
			VIP_LOG_ERROR("librir: missing get_default_west_data_dir");
			delete librir;
			return librir = nullptr;
		}

		librir->set_temp_directory = (_set_temp_directory)westLib()->resolve("set_west_data_dir");
		if (!librir->set_temp_directory) {
			VIP_LOG_ERROR("librir: missing set_west_data_dir");
			delete librir;
			return librir = nullptr;
		}

		librir->get_full_cam_identifier_from_partial = (_get_full_cam_identifier_from_partial)westLib()->resolve("get_full_cam_identifier_from_partial");
		if (!librir->get_full_cam_identifier_from_partial) {
			VIP_LOG_ERROR("librir: missing get_full_cam_identifier_from_partial");
			delete librir;
			return librir = nullptr;
		}

		librir->has_camera_preloaded = (_has_camera_preloaded)westLib()->resolve("has_camera_preloaded");
		if (!librir->has_camera_preloaded) {
			VIP_LOG_ERROR("librir: missing has_camera_preloaded");
			delete librir;
			return librir = nullptr;
		}

		librir->get_camera_filename = (_get_camera_filename)westLib()->resolve("get_camera_filename");
		if (!librir->get_camera_filename) {
			VIP_LOG_ERROR("librir: missing get_camera_filename");
			delete librir;
			return librir = nullptr;
		}
		librir->load_roi_file = (_load_roi_file)westLib()->resolve("load_roi_file");
		if (!librir->load_roi_file) {
			VIP_LOG_ERROR("librir: missing load_roi_file");
			delete librir;
			return librir = nullptr;
		}
		librir->camera_file_size = (_camera_file_size)westLib()->resolve("camera_file_size");
		if (!librir->camera_file_size) {
			VIP_LOG_ERROR("librir: missing camera_file_size");
			delete librir;
			return librir = nullptr;
		}

		librir->current_thread_id = (_current_thread_id)westLib()->resolve("current_thread_id");
		if (!librir->current_thread_id) {
			VIP_LOG_ERROR("librir: missing current_thread_id");
			delete librir;
			return librir = nullptr;
		}
		librir->cancel_last_operation = (_cancel_last_operation)westLib()->resolve("cancel_last_operation");
		if (!librir->cancel_last_operation) {
			VIP_LOG_ERROR("librir: missing cancel_last_operation");
			delete librir;
			return librir = nullptr;
		}
		librir->close_all_operations = (_close_all_operations)westLib()->resolve("close_all_operations");
		if (!librir->close_all_operations) {
			VIP_LOG_ERROR("librir: missing close_all_operations");
			delete librir;
			return librir = nullptr;
		}

		librir->load_roi_result_file = (_load_roi_result_file)westLib()->resolve("load_roi_result_file");
		if (!librir->load_roi_result_file) {
			VIP_LOG_ERROR("librir: missing load_roi_result_file");
			delete librir;
			return librir = nullptr;
		}
		librir->open_calibration = (_open_calibration)westLib()->resolve("open_calibration");
		if (!librir->open_calibration) {
			VIP_LOG_ERROR("librir: missing open_calibration");
			delete librir;
			return librir = nullptr;
		}
		librir->open_calibration_from_view = (_open_calibration)westLib()->resolve("open_calibration_from_view");
		if (!librir->open_calibration_from_view) {
			VIP_LOG_ERROR("librir: missing open_calibration_from_view");
			delete librir;
			return librir = nullptr;
		}

		librir->apply_lut = (_apply_lut)westLib()->resolve("apply_lut");
		if (!librir->apply_lut) {
			VIP_LOG_ERROR("librir: missing apply_lut");

			delete librir;
			return librir = nullptr;
		}
		librir->close_calibration = (_close_calibration)westLib()->resolve("close_calibration");
		if (!librir->close_calibration) {
			VIP_LOG_ERROR("librir: missing close_calibration");
			delete librir;
			return librir = nullptr;
		}

		librir->get_ir_config_infos = (_get_ir_config_infos)westLib()->resolve("get_ir_config_infos");
		if (!librir->get_ir_config_infos) {
			VIP_LOG_ERROR("librir: missing get_ir_config_infos");

			delete librir;
			return librir = nullptr;
		}
		librir->flip_calibration = (_flip_calibration)westLib()->resolve("flip_calibration");
		if (!librir->flip_calibration) {
			VIP_LOG_ERROR("librir: missing flip_calibration");
			delete librir;
			return librir = nullptr;
		}
		librir->apply_full_calibration = (_apply_full_calibration)westLib()->resolve("apply_full_calibration");
		if (!librir->apply_full_calibration) {
			VIP_LOG_ERROR("librir: missing apply_full_calibration");
			delete librir;
			return librir = nullptr;
		}

		librir->load_network_config = (_load_network_config)westLib()->resolve("load_network_config");
		if (!librir->load_network_config) {
			VIP_LOG_ERROR("librir: missing load_network_config");
			delete librir;
			return librir = nullptr;
		}

		librir->get_roi_dir = (_get_dir)westLib()->resolve("get_roi_dir");
		if (!librir->get_roi_dir) {
			VIP_LOG_ERROR("librir: missing get_roi_dir");
			delete librir;
			return librir = nullptr;
		}
		librir->get_lut_dir = (_get_dir)westLib()->resolve("get_lut_dir");
		if (!librir->get_lut_dir) {
			VIP_LOG_ERROR("librir: missing get_lut_dir");
			delete librir;
			return librir = nullptr;
		}
		librir->get_nuc_dir = (_get_dir)westLib()->resolve("get_nuc_dir");
		if (!librir->get_nuc_dir) {
			VIP_LOG_ERROR("librir: missing get_nuc_dir");
			delete librir;
			return librir = nullptr;
		}
		librir->get_trans_dir = (_get_dir)westLib()->resolve("get_trans_dir");
		if (!librir->get_trans_dir) {
			VIP_LOG_ERROR("librir: missing get_trans_dir");
			delete librir;
			return librir = nullptr;
		}
		librir->get_opt_dir = (_get_dir)westLib()->resolve("get_opt_dir");
		if (!librir->get_opt_dir) {
			VIP_LOG_ERROR("librir: missing get_opt_dir");
			delete librir;
			return librir = nullptr;
		}
		librir->get_irout_dir = (_get_dir)westLib()->resolve("get_irout_dir");
		if (!librir->get_irout_dir) {
			VIP_LOG_ERROR("librir: missing get_irout_dir");
			delete librir;
			return librir = nullptr;
		}
		librir->get_phase_file = (_get_dir)westLib()->resolve("get_phase_file");
		if (!librir->get_phase_file) {
			VIP_LOG_ERROR("librir: missing get_phase_file");
			delete librir;
			return librir = nullptr;
		}
		librir->get_views = (_get_views)westLib()->resolve("get_views");
		if (!librir->get_views) {
			VIP_LOG_ERROR("librir: missing get_views");
			delete librir;
			return librir = nullptr;
		}

		librir->load_asservIR = (_load_asservIR)westLib()->resolve("load_asservIR");
		if (!librir->load_asservIR) {
			VIP_LOG_ERROR("librir: missing load_asservIR");
			delete librir;
			return librir = nullptr;
		}

		librir->pchrono = (_pchrono)westLib()->resolve("pchrono");
		if (!librir->pchrono) {
			VIP_LOG_ERROR("librir: missing pchrono");
			delete librir;
			return librir = nullptr;
		}

		librir->check_top_access = (_check_top_access)westLib()->resolve("check_top_access");
		if (!librir->check_top_access) {
			VIP_LOG_ERROR("librir: missing check_top_access");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_last_pulse = (_ts_last_pulse)westLib()->resolve("ts_last_pulse");
		if (!librir->ts_last_pulse) {
			VIP_LOG_ERROR("librir: missing ts_last_pulse");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_exists = (_ts_exists)westLib()->resolve("ts_exists");
		if (!librir->ts_exists) {
			VIP_LOG_ERROR("librir: missing ts_exists");

			delete librir;
			return librir = nullptr;
		}
		librir->ts_date = (_ts_date)westLib()->resolve("ts_date");
		if (!librir->ts_date) {
			VIP_LOG_ERROR("librir: missing ts_date");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_read_file = (_ts_read_file)westLib()->resolve("ts_read_file");
		if (!librir->ts_read_file) {
			VIP_LOG_ERROR("librir: missing ts_read_file");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_file_size = (_ts_file_size)westLib()->resolve("ts_file_size");
		if (!librir->ts_file_size) {
			VIP_LOG_ERROR("librir: missing ts_file_size");
			delete librir;
			return librir = nullptr;
		}

		librir->ts_read_diagnostics = (_ts_read_diagnostics)westLib()->resolve("ts_read_diagnostics");
		if (!librir->ts_read_diagnostics) {
			VIP_LOG_ERROR("librir: missing ts_read_diagnostics");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_read_signal_names = (_ts_read_signal_names)westLib()->resolve("ts_read_signal_names");
		if (!librir->ts_read_signal_names) {
			VIP_LOG_ERROR("librir: missing ts_read_signal_names");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_chrono_date = (_ts_chrono_date)westLib()->resolve("ts_chrono_date");
		if (!librir->ts_chrono_date) {
			VIP_LOG_ERROR("librir: missing ts_chrono_date");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_get_ignitron = (_ts_get_ignitron)westLib()->resolve("ts_get_ignitron");
		if (!librir->ts_get_ignitron) {
			VIP_LOG_ERROR("librir: missing ts_get_ignitron");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_read_signal = (_ts_read_signal)westLib()->resolve("ts_read_signal");
		if (!librir->ts_read_signal) {
			VIP_LOG_ERROR("librir: missing ts_read_signal");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_read_group_count = (_ts_read_group_count)westLib()->resolve("ts_read_group_count");
		if (!librir->set_print_function) {
			VIP_LOG_ERROR("librir: missing ts_read_group_count");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_read_signal_group = (_ts_read_signal_group)westLib()->resolve("ts_read_signal_group");
		if (!librir->ts_read_signal_group) {
			VIP_LOG_ERROR("librir: missing ts_read_signal_group");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_signal_description = (_ts_signal_description)westLib()->resolve("ts_signal_description");
		if (!librir->ts_signal_description) {
			VIP_LOG_ERROR("librir: missing ts_signal_description");
			delete librir;
			return librir = nullptr;
		}
		librir->ts_pulse_infos = (_ts_pulse_infos)westLib()->resolve("ts_pulse_infos");
		if (!librir->ts_pulse_infos) {
			VIP_LOG_ERROR("librir: missing ts_pulse_infos");
			delete librir;
			return librir = nullptr;
		}

		librir->get_camera_rroi_info = (_get_camera_rroi_info)westLib()->resolve("get_camera_rroi_info");
		if (!librir->get_camera_rroi_info) {
			VIP_LOG_ERROR("librir: missing get_camera_rroi_info");
			delete librir;
			return librir = nullptr;
		}

		librir->get_camera_count = (_get_camera_count)westLib()->resolve("get_camera_count");
		if (!librir->get_camera_count) {
			VIP_LOG_ERROR("librir: missing get_camera_count");
			delete librir;
			return librir = nullptr;
		}
		librir->get_camera_infos = (_get_camera_infos)westLib()->resolve("get_camera_infos");
		if (!librir->get_camera_infos) {
			VIP_LOG_ERROR("librir: missing get_camera_infos");
			delete librir;
			return librir = nullptr;
		}
		librir->get_camera_index = (_get_camera_index)westLib()->resolve("get_camera_index");
		if (!librir->get_camera_index) {
			VIP_LOG_ERROR("librir: missing get_camera_index");
			delete librir;
			return librir = nullptr;
		}
		librir->open_camera = (_open_camera)westLib()->resolve("open_camera");
		if (!librir->open_camera) {
			VIP_LOG_ERROR("librir: missing open_camera");
			delete librir;
			return librir = nullptr;
		}
		librir->open_with_filename = (_open_with_filename)westLib()->resolve("open_with_filename");
		if (!librir->open_with_filename) {
			VIP_LOG_ERROR("librir: missing open_camera_filename");
			delete librir;
			return librir = nullptr;
		}
	}

	vip_debug("Read all functions done\n");

	return librir;
}

QByteArray VipLibRIR::getLastError() const
{
	QByteArray err(100, 0);
	int len = err.size();
	int cr = get_last_log_error(err.data(), &len);
	if (cr != 0 && len > err.size()) {
		err = QByteArray(len, 0);
		cr = get_last_log_error(err.data(), &len);
	}

	if (cr == 0)
		return QByteArray(err.data());
	return QByteArray();
}

QStringList VipLibRIR::availableCameraIdentifiers(int pulse)
{
	QStringList res;
	int count = get_camera_count(pulse);
	if (count > 0) {
		for (int i = 0; i < count; ++i) {
			// int num = 0;
			char name[100];
			memset(name, 0, sizeof(name));
			char identifier[100];
			memset(identifier, 0, sizeof(identifier));
			int exists = false;
			get_camera_infos(pulse, i, identifier, name, &exists);
			res.append(QString(identifier));
		}
	}
	return res;
}

QVariantMap VipLibRIR::getAttributes(int camera)
{
	int count = get_attribute_count(camera);
	if (count <= 0)
		return QVariantMap();

	QVariantMap res;
	std::vector<char> key(200);
	std::vector<char> value(200);
	int klen = 200;
	int vlen = 200;
	for (int i = 0; i < count; ++i) {
		int tmp = get_attribute(camera, i, key.data(), &klen, value.data(), &vlen);

		if (tmp == -2) {
			key.resize(klen);
			value.resize(vlen);
			tmp = get_attribute(camera, i, key.data(), &klen, value.data(), &vlen);
		}

		if (tmp < 0)
			return res;
		res.insert(std::string(key.data(), key.data() + klen).c_str(), QString(std::string(value.data(), value.data() + vlen).c_str()));
	}
	return res;
}
QVariantMap VipLibRIR::getGlobalAttributesAsString(int camera)
{
	int count = get_global_attribute_count(camera);
	if (count <= 0)
		return QVariantMap();

	QVariantMap res;
	std::string key(200, (char)0);
	std::string value(200, (char)0);
	int klen, vlen;
	for (int i = 0; i < count; ++i) {
		klen = vlen = 200;
		int tmp = get_global_attribute(camera, i, (char*)key.data(), &klen, (char*)value.data(), &vlen);

		if (tmp == -2) {
			key.resize(klen, (char)0);
			value.resize(vlen, (char)0);
			tmp = get_global_attribute(camera, i, (char*)key.data(), &klen, (char*)value.data(), &vlen);
		}

		if (tmp < 0)
			return res;
		res.insert(std::string(key.data(), key.data() + klen).c_str(), QString(std::string(value.data(), value.data() + vlen).c_str()));
	}
	return res;
}

QVariantMap VipLibRIR::getGlobalAttributesAsRawData(int camera)
{
	int count = get_global_attribute_count(camera);
	if (count <= 0)
		return QVariantMap();

	QVariantMap res;
	std::vector<char> key(200);
	std::vector<char> value(200);
	int klen = 200;
	int vlen = 200;
	for (int i = 0; i < count; ++i) {
		int tmp = get_global_attribute(camera, i, key.data(), &klen, value.data(), &vlen);

		if (tmp == -2) {
			key.resize(klen);
			value.resize(vlen);
			tmp = get_global_attribute(camera, i, key.data(), &klen, value.data(), &vlen);
		}
		else {
			key.resize(klen);
			value.resize(vlen);
		}

		if (tmp < 0)
			return res;
		QByteArray data(value.data(), (int)value.size());
		res.insert(std::string(key.data(), key.data() + klen).c_str(), data);
	}
	return res;
}

bool VipLibRIR::hasWESTFeatures() const
{
	return westLib() != nullptr;
}