<?php

function tap_preprocess_tap_tourml_tour(&$vars) {
    $tour                   = $vars['tour'];
    $vars['metadata']       = '';
    $vars['stops']          = '';
    $vars['assets']         = '';
    $vars['connections']    = '';

    if (isset($tour->tour['items'])) {
        foreach ($tour->tour['items'] as $tour_item) {
            $vars['metadata'] .= $tour_item;
        }
    }

    foreach ($tour->stops as $stop) {
        $vars['stops'] .= $stop;
    }

    foreach ($tour->assets as $asset) {
        $vars['assets'] .= $asset['rendered'];
    }

    foreach ($tour->connections as $connection) {
        $vars['connections'] .= $connection;
    }
}

function tap_preprocess_tap_tourml_root_stop_entityreference(&$vars) {
    $vars['real_id'] = isset($vars['items'][0]['target_id']) ? $vars['items'][0]['target_id'] : '';
}

/********************************
 * Stops
 */
function tap_preprocess_tap_tourml_stop(&$vars) {
}

/********************************
 * Assets
 */

function tap_preprocess_tap_tourml_asset(&$vars) {
    $properties         = array();
    $vars['source']     = FALSE;
    $vars['content']    = FALSE;


    // Handle File fields
    if (isset($vars['asset']['fid'])) {
        // Media fields nest the file object inside of the asset.
        // So if we run into one we need to merge it into the 
        // parent array.
        if (isset($vars['asset']['file'])) {
            $vars['asset'] = array_merge($vars['asset'], (array)$vars['asset']['file']);
        }
        if ($vars['absolute']) {
            $vars['path'] = file_create_url($vars['asset']['uri']);
        } else {
            list($schema, $vars['path']) = explode('://', $vars['asset']['uri']);
        }
        $vars['source'] = TRUE; 
    }

    // Handle text fields
    if (isset($vars['asset']['value'])) {
        $vars['content'] = TRUE;
    }

    if (array_key_exists('properties', $vars['asset'])) {
      if (is_array($vars['asset']['properties'])) {
        foreach($vars['asset']['properties'] as $name => $value) {
          $properties[] = theme('tap_tourml_property', array('name' => $name, 'value' => $value));
        }
      }
    }

    $vars['properties'] = theme('tap_tourml_property_set', array('properties' => $properties));
}

function theme_tap_tourml_asset_file(&$vars)
{
    return theme('tap_tourml_asset_ref', $vars);
}

function theme_tap_tourml_asset_image(&$vars) {
    return theme('tap_tourml_asset_ref', $vars);
}

function theme_tap_tourml_asset_text(&$vars) {
    return theme('tap_tourml_asset_ref', $vars);
}

function theme_tap_tourml_asset_text_long(&$vars) {
    return theme('tap_tourml_asset_ref', $vars);
}

function theme_tap_tourml_asset_text_with_summary(&$vars) {
    return theme('tap_tourml_asset_ref', $vars);
}

/********************************
 * Properties
 */

function tap_preprocess_tap_tourml_property_set(&$vars) {
}

function tap_preprocess_tap_tourml_property(&$vars) {
    $vars['attributes'] = drupal_attributes($vars['attributes']);
}

function _theme_tap_tourml_property($vars) {
    $properties = '';
    foreach($vars['items'] as $item) {
        $properties .= theme('tap_tourml_property', $item);
    }

    return $properties;
}

/**
 * these are all wrappers to _theme_tap_tourml_property() to handle the different field types
 */
function theme_tap_tourml_property_text(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_list_boolean(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_date(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_datetime(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_datestamp(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_number_decimal(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_number_float(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_number_integer(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_list_float(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_list_integer(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_list_text(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_text_long(&$vars) {
    return _theme_tap_tourml_property($vars);
}

function theme_tap_tourml_property_taxonomy_term_reference(&$vars) {
    foreach($vars['items'] as $id => $item) {
        $vars['items'][$id]['value'] = taxonomy_term_load($item['tid'])->name;
    }
    return _theme_tap_tourml_property($vars);
}

/********************************
 * Titles
 */

function _theme_tap_tourml_title($vars) {
    global $language;

    $properties = '';
    foreach($vars['items'] as $item) {
        $item['language'] = $language->language;
        $properties .= theme('tap_tourml_title', $item);
    }

    return $properties;
}

function theme_tap_tourml_title_text(&$vars) {
    return _theme_tap_tourml_title($vars); 
}

function theme_tap_tourml_title_text_long(&$vars) {
    return _theme_tap_tourml_title($vars); 
}

/********************************
 * Descriptions
 */

function _theme_tap_tourml_description($vars) {
    global $language;

    $properties = '';
    foreach($vars['items'] as $item) {
        $item['language'] = $language->language;
        $properties .= theme('tap_tourml_description', $item);
    }

    return $properties;
}

function theme_tap_tourml_description_text(&$vars) {
    return _theme_tap_tourml_description($vars);
}

function theme_tap_tourml_description_text_long(&$vars) {
    return _theme_tap_tourml_description($vars);
}

function theme_tap_tourml_description_text_with_summary(&$vars) {
    return _theme_tap_tourml_description($vars);
}

/********************************
 * Connections
 */

function tap_preprocess_tap_tourml_connection_entityreference(&$vars)
{
    foreach($vars['items'] as $k => $v)
    {
        $vars['items'][$k]['destId'] = $v['target_id'];
    }
}
