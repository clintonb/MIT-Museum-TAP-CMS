<?php

/**
 *
 * Export a downloadable tourml file
 * @param unknown_type $node
 */
function tap_export_tourml($node, $download = TRUE) {
	$title = _clean_file_text($node->title);

	$tourml = tap_tourml_render($node);

    header('Cache-Control: must-revalidate, post-check=0, pre-check=0');
    header('Content-Type: text/xml');

    if ($download) {
        header('Content-Disposition: attachment; filename="' . $title . '.xml"');
    }

    die(print $tourml);
}

/**
 * Export a zipped bundle with all assets
 * @param $node
 * A node object
 */
function tap_export_bundle($node) {
	$title = _clean_file_text($node->title);

	// Check for presense of zip extension
    if (!class_exists('ZipArchive')) {
        drupal_set_message("You must install the php zip extension to export bundles.", 'error');
        return;
    }

    // Delete old bundle from node
    if (isset($node->field_tour_bundle[$node->language][0]['fid'])) {
        $file = file_load($node->field_tour_bundle[$node->language][0]['fid']);
        $file->status = 0;
        file_field_delete_file($node->field_tour_bundle[$node->language][0], 'field_tour_bundle', 'node', $node->nid);
        unset($node->field_tour_bundle[$node->language][0]);
    }

    $path = file_directory_temp() . "/$title-bundle.zip";

    // If we have a stale tmp file delete it
    if (file_exists($path)) {
        unlink($path);
    }

    $bundle = _zip_init($path, ZIPARCHIVE::CREATE);

    $bdir = "$title.bundle";
    $bundle->addEmptyDir($bdir);

    // Add tourml document
    $settings = array(
        'return_object' => TRUE,
        'absolute'      => FALSE,
    );
    $tourml         = tap_tourml_render($node, $settings);
    $operations[]   = array('tap_export_bundle_string', array($path, $bdir, $tourml->getTourML()));

    // Add file assets
    foreach($tourml->assets as $asset) {
        if (isset($asset['fid'])) {
            $operations[] = array('tap_export_bundle_asset', array($path, $bdir, $asset['uri']));
        }
    }

    $operations[]   = array('tap_export_bundle_tour_save', array($node, $path));

    $batch = array(
        'title'         => t('TourML Bundle Export'),
        'operations'    => $operations,
        'finished'      => 'tap_export_bundle_finish',
        'file'          => drupal_get_path('module', 'tap') . '/tap.export.inc',
    );

    batch_set($batch);
}
/**
 * Bundle a string into a file inside of a zip archive
 *
 * @param $path
 * Path to the zip archive
 * @param $dir
 * The directory to save the asset in
 * @param $string
 * The string to insert into the file
 */
function tap_export_bundle_string($path, $dir, $string, &$context) {
    $bundle = _zip_init($path);
    $bundle->addFromString("$dir/tour.xml" , $string);
}

/**
 * Bundle a string into a file inside of a zip archive
 *
 * @param $path
 * Path to the zip archive
 * @param $dir
 * The directory to save the asset in
 * @param $uri
 * the uri of the file to store in the zip file
 */
function tap_export_bundle_asset($path, $dir, $uri, &$context) {
    $bundle = _zip_init($path);
    list($schema, $file_path) = explode('://', $uri);
    $real_path = drupal_realpath($uri);
    $bundle->addFile($real_path, $dir . '/' . $file_path);

    $context['results'][] = $file_path;
}

function tap_export_bundle_tour_save($node, $file_path, &$context) {
    global $user;
    $settings       = field_info_instance('node', 'field_tour_bundle', 'tour');
    $dest           = 'public://' . $settings['settings']['file_directory'] . '/' . basename($file_path);
    $trans          = db_transaction();

    try {
        // Begin building file object.
        $file           = new stdClass();
        $file->uid      = $user->uid;
        $file->status   = 0;
        $file->filename = trim(basename($file_path, '.'));
        $file->uri      = $file_path;
        $file->filemime = file_get_mimetype($file->filename);
        $file->filesize = filesize($file_path);

        if (rename($file_path, drupal_realpath($dest))) {
            $file->uri      = $dest;

            drupal_chmod($file->uri);
            if ($file = file_save($file)) {
                $node->field_tour_bundle[$node->language][0] = (array)$file;
                $node->field_tour_bundle[$node->language][0]['display'] = 1;
                node_save($node);
            }
        } else {
            throw new Exception('Unable to move uploaded file');
        }
    }
    catch(Exception $e) {
        $message = 'Error creating bundle: ' . basename($file_path);
        watchdog('tap', $message, array(), WATCHDOG_ERROR);
        drupal_set_message($message, 'error');
        $trans->rollback();
    }
}

function tap_export_bundle_finish($success, $results, $operations) {
    if ($success) {
        $message = 'Bundle has been created with ' . format_plural(count($results), 'One file included', '@count files included');
    } else {
        $message = t('An error occurred');
    }

    drupal_set_message($message);
}
