/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#ifndef VIP_LIB_RIR
#define VIP_LIB_RIR

#include <qlibrary.h>
#include <qmap.h>
#include <qvariant.h>

#include "VipConfig.h"

/// @brief Singleton class that wraps the librir library (https://github.com/IRFM/librir)
///
/// In order to work properly, the librir folder must be located next to the Thermavip binaries.
///
/// This class tries to load the WEST related components of librir, even if they do not exist
/// on the provided librir version. In this case, a simple warning message is logged.
///
class VIP_ANNOTATION_EXPORT VipLibRIR
{
	bool initialized;
	VipLibRIR();

public:
	~VipLibRIR();
	static VipLibRIR* instance();

	typedef void (*print_function)(int, const char*);
	typedef void (*_set_print_function)(print_function);
	typedef void (*_disable_print)();
	typedef void (*_reset_print_functions)();
	typedef int (*_get_last_log_error)(char* text, int* len);
	typedef int (*_check_top_access)(int, int);
	typedef int (*_ts_last_pulse)(int* pulse);
	typedef int (*_ts_exists)(int pulse, const char* name);
	typedef int (*_ts_date)(int pulse, char* date);
	typedef int (*_ts_read_file)(int pulse, const char* dataName, const char* outFileName);
	typedef int (*_ts_read_diagnostics)(int pulse, char* diags, int* length);
	typedef int (*_ts_read_signal_names)(int pulse, const char* diag_name, char* signal_names, int* length);
	typedef int (*_ts_chrono_date)(int pulse, const char* code, int occurence, float* res);
	typedef int (*_ts_get_ignitron)(int pulse, qint64* ignitron);
	typedef int (*_ts_file_size)(int, const char*, unsigned*);
	typedef int (*_ts_read_signal)(int pulse, const char* signal_name, double* x, double* y, int* sample_count, char* y_unit, char* date);
	typedef int (*_ts_read_group_count)(int pulse, const char* group_name, int* signal_count, int* sample_count);
	typedef int (*_ts_read_signal_group)(int pulse, const char* group_name, double* x, double* y, int* total_sample_count, int* sample_count, int* signal_count, char* y_unit, char* date);
	typedef int (*_ts_signal_description)(const char* signal_name, char* description, int* description_length);
	typedef int (*_ts_read_numeric_param)(int pulse, const char* NmProd, const char* NmObj, const char* NmPar, double* out_value);
	typedef int (*_ts_pulse_infos)(int pulse,
				       double* duration,
				       double* IP,
				       double* IP_t,
				       double* LH1,
				       double* LH1_t,
				       double* LH2,
				       double* LH2_t,
				       double* LHT,
				       double* LHT_t,
				       double* ICR1,
				       double* ICR1_t,
				       double* ICR2,
				       double* ICR2_t,
				       double* ICR4,
				       double* ICR4_t,
				       double* ICRT,
				       double* ICRT_t,
				       double* MaxPow,
				       double* MaxPow_t,
				       double* TotEnergy,
				       char* config,
				       char* date,
				       double* disruption);
	typedef int (*_get_camera_rroi_info)(int pulse, const char* identifier, int* maxT, int64_t* maxTtime, int* hasOverheat, double* sizeKB);
	typedef int (*_get_camera_count)(int pulse);
	typedef int (*_get_camera_infos)(int pulse, int index, char* unique_identifier, char* comprehensive_name, int* exists);
	typedef int (*_get_camera_index)(int pulse, const char* unique_identifier);
	typedef int (*_open_camera)(int pulse, const char* identifier);
	typedef int (*_open_camera_file)(const char* filename, int* type);
	typedef int (*_has_camera_preloaded)(int pulse, const char* identifier);
	typedef int (*_get_camera_filename)(int pulse, const char* identifier, char* dst);
	typedef int (*_set_global_emissivity)(int camera, float emi);
	typedef int (*_set_emissivity)(int camera, float* emi, int size);
	typedef int (*_support_emissivity)(int camera);
	typedef int (*_get_emissivity)(int camera, float* emi, int size);
	typedef int (*_camera_saturate)(int cam);

	typedef int (*_get_full_cam_identifier_from_partial)(int pulse, char* partial_in, char* full_out);

	typedef int (*_calibration_files)(int camera, char* dst, int* dstSize);

	typedef int (*_set_optical_temperature)(int camera, unsigned short temp_C);
	typedef unsigned short (*_get_optical_temperature)(int camera);
	typedef int (*_support_optical_temperature)(int camera);

	typedef int (*_get_image_count)(int camera);
	typedef int (*_get_image_time)(int camera, int pos, qint64* time);
	typedef int (*_get_image_size)(int camera, int* width, int* height);
	typedef int (*_supported_calibrations)(int camera, int* count);
	typedef int (*_calibration_name)(int camera, int calibration, char* name);
	typedef int (*_load_image)(int camera, int pos, int calibration, unsigned short* pixels);
	typedef int (*_get_last_image_raw_value)(int camera, int x, int y, unsigned short* value);
	typedef int (*_close_camera)(int camera);
	typedef int (*_get_filename)(int cam, char* filename);

	typedef int (*_get_temp_directory)(char*);
	typedef int (*_set_temp_directory)(char*);
	typedef void (*_enable_delete_temp_dir)(int);
	typedef int (*_delete_temp_dir_enabled)();
	typedef unsigned (*_camera_file_size)(int pulse, const char* identifier);

	typedef int (*_load_roi_result_file)(int pulse, const char* identifier, const char* out_file);
	typedef int (*_load_roi_file)(int pulse, const char* identifier, const char* out_file);

	// connection management
	typedef size_t (*_current_thread_id)();
	typedef void (*_cancel_last_operation)(size_t thread_id);
	typedef void (*_close_all_operations)();

	typedef int (*_get_attribute_count)(int);
	typedef int (*_get_attribute)(int camera, int index, char* key, int* key_len, char* value, int* value_len);

	typedef int (*_enable_bad_pixels)(int, int);
	typedef int (*_bad_pixels_enabled)(int);

	typedef int (*_load_motion_correction_file)(int cam, const char* filename);
	typedef int (*_enable_motion_correction)(int cam, int enable);
	typedef int (*_motion_correction_enabled)(int cam);

	typedef void* (*_open_video_write)(const char* filename, int width, int height, int rate, int method, int clevel);
	typedef int (*_image_write)(void* writter, unsigned short* img, int64_t time);
	typedef int64_t (*_close_video)(void* writter);

	typedef int (*_calibrate_image)(int cam, unsigned short* img, float* out, int size, int calib);
	typedef int (*_calibrate_image_inplace)(int cam, unsigned short* img, int size, int calib);
	typedef int (*_open_calibration)(int pulse, const char* cam);
	typedef int (*_apply_lut)(int calib, unsigned short* img, int size);
	typedef void (*_close_calibration)(int);

	typedef int (
	  *_get_ir_config_infos)(int pulse, const char* view, char* dir, int* fpga, char* cam, int* flip_rl, int* flip_ud, char* roi, char* lut, char* trhub, char* trfut, char* trmir, char* lopt);
	typedef int (*_flip_calibration)(int calib, int flip_rl, int flip_ud);
	typedef int (*_apply_full_calibration)(int calib, unsigned short* img, int size);

	typedef int (*_load_network_config)(const char* view, char* fpga_ip, int* fpga_port, char* fpga_mac, char* wms_ip, int* wms_port, char* wms_mac);
	typedef void (*_get_dir)(char* out);

	typedef int64_t (*_zstd_compress_bound)(int64_t srcSize);
	typedef int64_t (*_zstd_decompress_bound)(char* src, int64_t srcSize);
	typedef int64_t (*_zstd_compress)(char* src, int64_t srcSize, char* dst, int64_t dstSize, int level);
	typedef int64_t (*_zstd_decompress)(char* src, int64_t srcSize, char* dst, int64_t dstSize);

	typedef int (*_h264_open_file)(const char* filename, int width, int height, int lossy_height);
	typedef void (*_h264_close_file)(int file);
	typedef int (*_h264_set_parameter)(int file, const char* param, const char* value);
	typedef int (*_h264_set_global_attributes)(int file, int attribute_count, char* keys, int* key_lens, char* values, int* value_lens);
	typedef int (*_h264_add_image_lossless)(int file, unsigned short* img, int64_t timestamps_ns, int attribute_count, char* keys, int* key_lens, char* values, int* value_lens);
	typedef int (*_h264_add_image_lossy)(int file, unsigned short* img_DL, int64_t timestamps_ns, int attribute_count, char* keys, int* key_lens, char* values, int* value_lens);

	typedef int (*_get_views)(int pulse, char* views);

	typedef int (*_load_asservIR)(int pulse, const char* antenna, char* views, int* roi_index, int* start_asserv, int* end_asserv, int* enabled);

	typedef int (*_unzip)(const char* infile, const char* outdir);

	typedef int (*_pchrono)(int pulse, char* out, int* out_size);
	typedef int (*_open_with_filename)(const char* filename);

	typedef int (*_get_table_names)(int cam, char* dst, int* dst_size);
	typedef int (*_get_table)(int cam, const char* name, float* dst, int* dst_size);

	typedef int (*_apply_calibration_nuc)(int cam, int enable);
	typedef int (*_is_calibration_nuc)(int cam);
	typedef int (*_convert_hcc_file)(const char* hcc_file,
					 const char* lut_file,
					 const char* tr_fut,
					 const char* tr_hub,
					 const char* tr_mir,
					 const char* lopt,
					 const char* nuc,
					 int64_t start_timestamp_ns,
					 const char* view,
					 const char* out_file);
	typedef int (*_hcc_extract_times_and_fw_pos)(int cam, int64_t* times, int* pos);
	typedef int (*_hcc_extract_all_fw_pos)(int cam, int* pos, int* pos_count);

	typedef void (*_set_hcc_lut_file)(const char* filename);
	typedef void (*_get_hcc_lut_file)(char* filename);

	typedef void (*_set_hcc_lopt_file)(const char* filename);
	typedef void (*_get_hcc_lopt_file)(char* filename);

	typedef void (*_set_hcc_nuc_file)(const char* filename);
	typedef void (*_get_hcc_nuc_file)(char* filename);

	typedef void (*_set_hcc_trhub_file)(const char* filename);
	typedef void (*_get_hcc_trhub_file)(char* filename);

	typedef void (*_set_hcc_trmir_file)(const char* filename);
	typedef void (*_get_hcc_trmir_file)(char* filename);

	typedef void (*_set_hcc_trfut_file)(const char* filename);
	typedef void (*_get_hcc_trfut_file)(char* filename);

	typedef int (*_attrs_open_file)(const char* filename);
	typedef void (*_attrs_close)(int handle);
	typedef int (*_attrs_image_count)(int handle);
	typedef int (*_attrs_global_attribute_count)(int handle);
	typedef int (*_attrs_global_attribute_name)(int handle, int pos, char* name, int* len);
	typedef int (*_attrs_global_attribute_value)(int handle, int pos, char* value, int* len);
	typedef int (*_attrs_frame_attribute_count)(int handle, int frame);
	typedef int (*_attrs_frame_attribute_name)(int handle, int frame, int pos, char* name, int* len);
	typedef int (*_attrs_frame_attribute_value)(int handle, int frame, int pos, char* value, int* len);
	typedef int (*_attrs_frame_timestamp)(int handle, int frame, int64_t* time);
	typedef int (*_attrs_timestamps)(int handle, int64_t* time);

	_set_print_function set_print_function;
	_disable_print disable_print;
	_reset_print_functions reset_print_functions;
	_get_last_log_error get_last_log_error;
	_check_top_access check_top_access;
	_ts_last_pulse ts_last_pulse;
	_ts_exists ts_exists;
	_ts_date ts_date;
	_ts_read_file ts_read_file;
	_ts_file_size ts_file_size;
	_ts_read_diagnostics ts_read_diagnostics;
	_ts_read_signal_names ts_read_signal_names;
	_ts_chrono_date ts_chrono_date;
	_ts_get_ignitron ts_get_ignitron;
	_ts_read_signal ts_read_signal;
	_ts_read_group_count ts_read_group_count;
	_ts_read_signal_group ts_read_signal_group;
	_ts_signal_description ts_signal_description;
	_ts_read_numeric_param ts_read_numeric_param;
	_ts_pulse_infos ts_pulse_infos;
	_get_camera_rroi_info get_camera_rroi_info;
	_get_camera_count get_camera_count;
	_get_camera_infos get_camera_infos;
	_get_camera_index get_camera_index;
	_open_camera open_camera;
	_has_camera_preloaded has_camera_preloaded;
	_get_camera_filename get_camera_filename;
	_open_camera_file open_camera_file;
	_open_with_filename open_with_filename;
	_set_global_emissivity set_global_emissivity;
	_set_emissivity set_emissivity;
	_support_emissivity support_emissivity;
	_get_emissivity get_emissivity;
	_set_optical_temperature set_optical_temperature;
	_get_optical_temperature get_optical_temperature;
	_set_optical_temperature set_STEFI_temperature;
	_get_optical_temperature get_STEFI_temperature;
	_support_optical_temperature support_optical_temperature;

	_load_motion_correction_file load_motion_correction_file;
	_enable_motion_correction enable_motion_correction;
	_motion_correction_enabled motion_correction_enabled;

	_get_full_cam_identifier_from_partial get_full_cam_identifier_from_partial;

	_get_image_count get_image_count;
	_get_image_time get_image_time;
	_get_image_size get_image_size;
	_supported_calibrations supported_calibrations;
	_calibration_name calibration_name;
	_load_image load_image;
	_get_last_image_raw_value get_last_image_raw_value;
	_close_camera close_camera;
	_get_filename get_filename;
	_get_temp_directory get_temp_directory;
	_get_temp_directory get_default_temp_directory;
	_set_temp_directory set_temp_directory;
	//_enable_delete_temp_dir enable_delete_temp_dir;
	//_delete_temp_dir_enabled delete_temp_dir_enabled;

	_calibration_files calibration_files;

	_camera_saturate camera_saturate;
	_camera_file_size camera_file_size;

	_load_roi_result_file load_roi_result_file;
	_load_roi_file load_roi_file;

	_current_thread_id current_thread_id;
	_cancel_last_operation cancel_last_operation;
	_close_all_operations close_all_operations;

	_get_attribute_count get_attribute_count;
	_get_attribute get_attribute;
	_get_attribute_count get_global_attribute_count;
	_get_attribute get_global_attribute;

	_enable_bad_pixels enable_bad_pixels;
	_bad_pixels_enabled bad_pixels_enabled;

	//_open_video_write open_video_write;
	//_image_write image_write;
	//_close_video close_video;

	_calibrate_image calibrate_image;
	_calibrate_image_inplace calibrate_image_inplace;
	_open_calibration open_calibration;
	_open_calibration open_calibration_from_view;
	_apply_lut apply_lut;
	_close_calibration close_calibration;

	_get_ir_config_infos get_ir_config_infos;
	_flip_calibration flip_calibration;
	_apply_full_calibration apply_full_calibration;

	_load_network_config load_network_config;
	_get_dir get_roi_dir;
	_get_dir get_lut_dir;
	_get_dir get_nuc_dir;
	_get_dir get_trans_dir;
	_get_dir get_opt_dir;
	_get_dir get_irout_dir;
	_get_dir get_phase_file;

	_zstd_compress_bound zstd_compress_bound;
	_zstd_decompress_bound zstd_decompress_bound;
	_zstd_compress zstd_compress;
	_zstd_decompress zstd_decompress;

	_h264_open_file h264_open_file;
	_h264_close_file h264_close_file;
	_h264_set_parameter h264_set_parameter;
	_h264_set_global_attributes h264_set_global_attributes;
	_h264_add_image_lossless h264_add_image_lossless;
	_h264_add_image_lossy h264_add_image_lossy;

	_get_table_names get_table_names;
	_get_table get_table;

	_get_views get_views;

	_load_asservIR load_asservIR;

	_unzip unzip;

	_pchrono pchrono;

	_apply_calibration_nuc apply_calibration_nuc;
	_is_calibration_nuc is_calibration_nuc;
	_convert_hcc_file convert_hcc_file;
	_hcc_extract_times_and_fw_pos hcc_extract_times_and_fw_pos;
	_hcc_extract_all_fw_pos hcc_extract_all_fw_pos;

	_set_hcc_lut_file set_hcc_lut_file;
	_get_hcc_lut_file get_hcc_lut_file;

	_set_hcc_lopt_file set_hcc_lopt_file;
	_get_hcc_lopt_file get_hcc_lopt_file;

	_set_hcc_nuc_file set_hcc_nuc_file;
	_get_hcc_nuc_file get_hcc_nuc_file;

	_set_hcc_trhub_file set_hcc_trhub_file;
	_get_hcc_trhub_file get_hcc_trhub_file;

	_set_hcc_trmir_file set_hcc_trmir_file;
	_get_hcc_trmir_file get_hcc_trmir_file;

	_set_hcc_trfut_file set_hcc_trfut_file;
	_get_hcc_trfut_file get_hcc_trfut_file;

	_attrs_open_file attrs_open_file;
	_attrs_close attrs_close;
	_attrs_image_count attrs_image_count;
	_attrs_global_attribute_count attrs_global_attribute_count;
	_attrs_global_attribute_name attrs_global_attribute_name;
	_attrs_global_attribute_value attrs_global_attribute_value;
	_attrs_frame_attribute_count attrs_frame_attribute_count;
	_attrs_frame_attribute_name attrs_frame_attribute_name;
	_attrs_frame_attribute_value attrs_frame_attribute_value;
	_attrs_frame_timestamp attrs_frame_timestamp;
	_attrs_timestamps attrs_timestamps;

	// helper function
	QStringList availableCameraIdentifiers(int pulse);
	QVariantMap getAttributes(int camera);
	QVariantMap getGlobalAttributesAsString(int camera);
	QVariantMap getGlobalAttributesAsRawData(int camera);

	bool hasWESTFeatures() const;

	QByteArray getLastError() const;
};

#endif