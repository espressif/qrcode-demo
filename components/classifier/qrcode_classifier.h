/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file qrcode_classifier.h
 * @brief QR code classifier
 * This component of the demo is used to classify the contents of the QR code.
 * For example, if the QR code contains something that looks like Wi-Fi network credentials,
 * the classifier will return a string such as "wifi.png".
 * The classifier uses contents of the file specified in the config_path parameter.
 * The file contains a number of lines, each line has the form of:
 *
 *   <regular expression> <string>
 *
 * where <regular expression> will be used to match the contents of the QR code,
 * and <string> will be returned if the match is successful.
 */

/**
 * @brief Initialize the QR code classifier
 *
 * @param config_path  Path to the file with classifier configuration. See the description of the file format above.
 */
void classifier_init(const char *config_path);

/**
 * @brief Get the classification (e.g. picture name) from QR code data
 *
 * @param text  QR code data
 * @return const char*  Classification (e.g. picture name), as set in the configuration file
 */
const char *classifier_get_pic_from_qrcode_data(const char *text);

#ifdef __cplusplus
}
#endif