<?php

/**
 * @file
 * The entity translation user interface.
 */

/**
 * The entity translation settings form.
 */
function entity_translation_admin_form($form, $form_state) {
  $options = array();

  $form['locale_field_language_fallback'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable language fallback'),
    '#description' => t('When language fallback is enabled, if a translation is not available for the requested language an existing one will be displayed.'),
    '#default_value' => variable_get('locale_field_language_fallback', TRUE),
  );

  foreach (entity_get_info() as $entity_type => $info) {
    if ($info['fieldable']) {
      $options[$entity_type] = $info['label'];
    }
  }

  $form['entity_translation_entity_types'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Translatable entity types'),
    '#description' => t('Select which entities can be translated.'),
    '#options' => $options,
    '#default_value' => variable_get('entity_translation_entity_types', array()),
  );

  $form = system_settings_form($form);

  // Menu rebuilding needs to be performed after the system settings are saved.
  $form['#submit'][] = 'entity_translation_admin_form_submit';

  return $form;
}

/**
 * Submit handler for the entity translation settings form.
 */
function entity_translation_admin_form_submit($form, $form_state) {
  // Clear the entity info cache for the new entity translation settings.
  entity_info_cache_clear();
  menu_rebuild();
}

/**
 * Translations overview menu callback.
 */
function entity_translation_overview($entity_type, $entity) {
  // Entity translation and node translation share the same system path.
  if (entity_translation_node($entity_type, $entity)) {
    if (module_exists('i18n_node')) {
      module_load_include('inc', 'i18n_node', 'i18n_node.pages');
      return i18n_node_translation_overview($entity);
    }
    else {
      module_load_include('inc', 'translation', 'translation.pages');
      return translation_node_overview($entity);
    }
  }

  $handler = entity_translation_get_handler($entity_type, $entity);

  // Initialize translations if they are empty.
  $translations = $handler->getTranslations();
  if (empty($translations->original)) {
    $handler->initTranslations();
    $handler->saveTranslations();
  }

  // Ensure that we have a coherent status between entity language and field
  // languages.
  if ($handler->initOriginalTranslation()) {
    // FIXME!
    field_attach_presave($entity_type, $entity);
    field_attach_update($entity_type, $entity);
  }

  $header = array(t('Language'), t('Source language'), t('Translation'), t('Status'), t('Operations'));
  // @todo: Do we want only enabled languages here?
  $languages = language_list();
  $source = isset($_SESSION['entity_translation_source_language']) ? $_SESSION['entity_translation_source_language'] :  $translations->original;
  $base_path = $handler->getBasePath();
  $path = $handler->getViewPath();

  if ($path) {
    // If we have a view path defined for the current entity get the switch
    // links based on it.
    $links = language_negotiation_get_switch_links(LANGUAGE_TYPE_CONTENT, $path);
  }

  foreach ($languages as $language) {
    $options = array();
    $language_name = $language->name;
    $langcode = $language->language;

    if (isset($translations->data[$langcode])) {
      list($id, $vid, $bundle) = entity_extract_ids($entity_type, $entity);

      // Existing translation in the translation set: display status.
      $is_original = $langcode == $translations->original;
      $translation = $translations->data[$langcode];
      $label = _entity_translation_label($entity_type, $entity, $langcode);
      $link = isset($links->links[$langcode]['href']) ? $links->links[$langcode] : array('href' => $path, 'language' => $language);
      $row_title = l($label, $link['href'], $link);

      if (empty($link['href'])) {
        $row_title = $is_original ? $label : t('n/a');
      }

      $edit_path = $is_original ? $handler->getEditPath() : $base_path . '/translate/edit/' . $langcode;
      if ($edit_path && $handler->getAccess('update')) {
        $options[] = l($is_original ? t('edit') : t('edit translation'), $edit_path);
      }

      $status = $translation['status'] ? t('Published') : t('Not published');
      $status .= isset($translation['translate']) && $translation['translate'] ? ' - <span class="marker">' . t('outdated') . '</span>' : '';

      if ($is_original) {
        $language_name = t('<strong>@language_name</strong>', array('@language_name' => $language_name));
        $source_name = t('(original content)');
      }
      else {
        $source_name = $languages[$translation['source']]->name;
      }
    }
    else {
      // No such translation in the set yet: help user to create it.
      $row_title = $source_name = t('n/a');
      $add_path = "$base_path/translate/add/$langcode/$source";

      if ($source != $langcode && $handler->getAccess('update')) {
        list(, , $bundle) = entity_extract_ids($entity_type, $entity);
        $translatable = FALSE;

        foreach (field_info_instances($entity_type, $bundle) as $instance) {
          $field_name = $instance['field_name'];
          $field = field_info_field($field_name);
          if ($field['translatable']) {
            $translatable = TRUE;
            break;
          }
        }

        $options[] = $translatable ? l(t('add translation'), $add_path) : t('No translatable fields');
      }
      $status = t('Not translated');
    }
    $rows[] = array($language_name, $source_name, $row_title, $status, implode(" | ", $options));
  }

  drupal_set_title(t('Translations of %label', array('%label' => $handler->getLabel())), PASS_THROUGH);

  $build['entity_translation_overview'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
  );

  return $build;
}

/**
 * Return the appropriate entity label for the given language.
 */
function _entity_translation_label($entity_type, $entity, $langcode = NULL) {
  if (function_exists('title_entity_label')) {
    list (, , $bundle) = entity_extract_ids($entity_type, $entity);
    $entity_info = entity_get_info($entity_type);
    $legacy_field = $entity_info['entity keys']['label'];
    if (title_field_replacement_enabled($entity_type, $bundle, $legacy_field)) {
      return title_entity_label($entity, $entity_type, $langcode);
    }
  }
  return t('view');
}

/**
 * Translation adding/editing form.
 */
function entity_translation_edit_form($form, $form_state, $entity_type, $entity, $langcode, $source = NULL) {
  if (entity_translation_node($entity_type, $entity)) {
    drupal_goto("node/$entity->nid/translate");
  }

  $handler = entity_translation_get_handler($entity_type, $entity);

  $languages = language_list();
  $args = array('@label' => $handler->getLabel(), '@language' => t($languages[$langcode]->name));
  drupal_set_title(t('@label [@language translation]', $args));

  $translations = $handler->getTranslations();
  $new_translation = !isset($translations->data[$langcode]);

  $form = array(
    '#handler' => $handler,
    '#entity_type' => $entity_type,
    '#entity' => $entity,
    '#source' => $new_translation ? $source : $translations->data[$langcode]['source'],
    '#language' => $langcode,
  );

  // Display source language selector only if we are creating a new translation
  // and there are at least two translations available.
  if ($new_translation && count($translations->data) > 1) {
    $form['source_language'] = array(
      '#type' => 'fieldset',
      '#title' => t('Source language'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#tree' => TRUE,
      '#weight' => -22,
      'language' => array(
        '#type' => 'select',
        '#default_value' => $source,
        '#options' => array(),
      ),
      'submit' => array(
        '#type' => 'submit',
        '#value' => t('Change'),
        '#submit' => array('entity_translation_edit_form_source_language_submit'),
      ),
    );
    foreach (language_list() as $language) {
      if (isset($translations->data[$language->language])) {
        $form['source_language']['language']['#options'][$language->language] = t($language->name);
      }
    }
  }

  $translate = intval(isset($translations->data[$langcode]) && $translations->data[$langcode]['translate']);

  $form['translation'] = array(
    '#type' => 'fieldset',
    '#title' => t('Translation settings'),
    '#collapsible' => TRUE,
    '#collapsed' => !$translate,
    '#tree' => TRUE,
    '#weight' => -24,
  );
  $form['translation']['status'] = array(
    '#type' => 'checkbox',
    '#title' => t('This translation is published'),
    '#default_value' => !isset($translations->data[$langcode]) || $translations->data[$langcode]['status'],
    '#description' => t('When this option is unchecked, this translation will not be visible for non-administrators.'),
  );
  $form['translation']['translate'] = array(
    '#type' => 'checkbox',
    '#title' => t('This translation needs to be updated'),
    '#default_value' => $translate,
    '#description' => t('When this option is checked, this translation needs to be updated because the source post has changed. Uncheck when the translation is up to date again.'),
    '#disabled' => !$translate,
  );

  // If we are creating a new translation we need to retrieve form elements
  // populated with the source language values, but only if form is not being
  // rebuilt. In this case source values have already been populated, so we need
  // to preserve possible changes.
  $prepare_fields = $new_translation && !$form_state['rebuild'];
  if ($prepare_fields) {
    $source_form = array();
    $source_form_state = $form_state;
    field_attach_form($entity_type, $entity, $source_form, $source_form_state, $source);
  }
  field_attach_form($entity_type, $entity, $form, $form_state, $langcode);

  list(, , $bundle) = entity_extract_ids($entity_type, $entity);
  foreach (field_info_instances($entity_type, $bundle) as $instance) {
    $field_name = $instance['field_name'];
    $field = field_info_field($field_name);
    // If a field is not translatable remove it from the translation form.
    if (!$field['translatable']) {
      $form[$field_name]['#access'] = FALSE;
    }
    // If we are creating a new translation we have to change the form item
    // language information from source to target language, this way the
    // user can find the form items already populated with the source values
    // while the field form element holds the correct language information.
    elseif ($prepare_fields && !isset($entity->{$field_name}[$langcode]) && isset($source_form[$field_name][$source])) {
      $form[$field_name][$langcode] = $source_form[$field_name][$source];
      // Update #language keys in the field form subtree.
      array_walk_recursive($form[$field_name][$langcode], '_entity_translation_form_language', $langcode);
    }
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save translation'),
    '#validate' => array('entity_translation_edit_form_save_validate'),
    '#submit' => array('entity_translation_edit_form_save_submit'),
  );
  if (!$new_translation) {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete translation'),
      '#submit' => array('entity_translation_edit_form_delete_submit'),
    );
  }

  // URL alias widget.
  if (_entity_translation_path_enabled($handler)) {
    $alias = db_select('url_alias')
      ->fields('url_alias', array('alias'))
      ->condition('source', $handler->getViewPath())
      ->condition('language', $langcode)
      ->execute()
      ->fetchField();

    $form['path'] = array(
      '#type' => 'textfield',
      '#title' => t('URL alias'),
      '#default_value' => $alias,
      '#maxlength' => 255,
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#description' => t('Optionally specify an alternative URL by which this entity can be accessed. For example, type "about" when writing an about page. Use a relative path and don\'t add a trailing slash or the URL alias won\'t work.'),
      '#access' => user_access('create url aliases'),
      '#weight' => -20,
    );

    if (!empty($alias)) {
      $pid = db_select('url_alias')
        ->fields('url_alias', array('pid'))
        ->condition('alias', $alias)
        ->condition('language', $langcode)
        ->execute()
        ->fetchField();

      $form['path']['pid'] = array(
        '#type' => 'value',
        '#value' => $pid,
      );
    }
  }

  return $form;
}

/**
 * Helper callback: replace the source language with the given one.
 */
function _entity_translation_form_language(&$item, $key, $langcode) {
  if ($key === '#language') {
    $item = $langcode;
  }
}

/**
 * Submit handler for the source language selector.
 */
function entity_translation_edit_form_source_language_submit($form, &$form_state) {
  $handler = $form['#handler'];
  $langcode = $form_state['values']['source_language']['language'];
  $path = "{$handler->getBasePath()}/translate/add/{$form['#language']}/$langcode";
  $form_state['redirect'] = array('path' => $path);
  $languages = language_list();
  drupal_set_message(t('Source translation set to: %language', array('%language' => t($languages[$langcode]->name))));
}

/**
 * Validation handler for the translation saving.
 */
function entity_translation_edit_form_save_validate($form, &$form_state) {
  field_attach_form_validate($form['#entity_type'], $form['#entity'], $form, $form_state);
}

/**
 * Submit handler for the translation saving.
 */
function entity_translation_edit_form_save_submit($form, &$form_state) {
  $handler = $form['#handler'];

  $translation = array(
    'translate' => $form_state['values']['translation']['translate'],
    'status' => $form_state['values']['translation']['status'],
    'language' => $form['#language'],
    'source' => $form['#source'],
  );

  $handler->setTranslation($translation, $form_state['values']);
  field_attach_submit($form['#entity_type'], $form['#entity'], $form, $form_state);
  field_attach_presave($form['#entity_type'], $form['#entity']);
  field_attach_update($form['#entity_type'], $form['#entity']);

  // Update URL alias.
  if (_entity_translation_path_enabled($handler) && (user_access('create url aliases') || user_access('administer url aliases'))) {
    $path = array(
      'source' => $handler->getViewPath(),
      'alias' => $form_state['values']['path'],
      'pid' => isset($form_state['values']['pid']) ? $form_state['values']['pid'] : NULL,
      'language' => $form['#language'],
    );
    if (!empty($path['pid']) && empty($path['alias'])) {
      path_delete($path['pid']);
    }
    if (!empty($path['alias'])) {
      path_save($path);
    }
  }

  $form_state['redirect'] = "{$handler->getBasePath()}/translate";
}

/**
 * Helper function to check if the path support is enabled.
 */
function _entity_translation_path_enabled(EntityTranslationHandlerInterface $handler) {
  return $handler->isAliasEnabled() && module_exists('path');
}

/**
 * Submit handler for the translation deletion.
 */
function entity_translation_edit_form_delete_submit($form, &$form_state) {
  $form_state['redirect'] = "{$form['#handler']->getBasePath()}/translate/delete/{$form['#language']}";
}

/**
 * Translation deletion confirmation form.
 */
function entity_translation_delete_confirm($form, $form_state, $entity_type, $entity, $langcode) {
  $handler = entity_translation_get_handler($entity_type, $entity);
  $languages = language_list();

  $form = array(
    '#handler' => $handler,
    '#entity_type' => $entity_type,
    '#entity' => $entity,
    '#language' => $langcode,
  );

  return confirm_form(
    $form,
    t('Are you sure you want to delete the @language translation of %label?', array('@language' => $languages[$langcode]->name, '%label' => $handler->getLabel())),
    "{$handler->getBasePath()}/translate/edit/$langcode",
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for the translation deletion confirmation.
 */
function entity_translation_delete_confirm_submit($form, &$form_state) {
  $handler = $form['#handler'];

  $handler->removeTranslation($form['#language']);
  field_attach_update($form['#entity_type'], $form['#entity']);

  if (isset($_SESSION['entity_translation_source_language']) && $form['#language'] == $_SESSION['entity_translation_source_language']) {
    unset($_SESSION['entity_translation_source_language']);
  }

  $form_state['redirect'] = "{$handler->getBasePath()}/translate";
}
