<?php

/**
 * @file
 * Callbacks to display a log of a find & replace operation.
 */

/**
 * Page callback to display a log of a find & replace operation.
 *
 * @param array $find_replace
 *   The Find & Replace database row as array.
 *
 * @return array
 *   Render array for page.
 */
function islandora_find_replace_operation_log($find_replace) {
  if ($find_replace['state'] != 'complete') {
    drupal_set_message(t('The Find & Replace log is not available.'), 'error');
    drupal_goto('admin/islandora/tools/find-replace/find');
  }

  $revisions = unserialize($find_replace['revisions']);

  $dsid = $find_replace['dsid'];

  $diff_exists = module_exists('islandora_pretty_text_diff');
  $success_header = array(
    t('Label'),
    t('Datastream Versions'),
  );
  if ($diff_exists) {
    $success_header[] = t('Datastream Diff');
  }

  $success_rows = array();
  foreach ($revisions['success'] as $pid => $info) {
    $object = islandora_object_load($pid);
    $success_rows[$pid] = array(
      l($object->label, 'islandora/object/' . $object->id,
        array('attributes' => array('target' => '_blank'))),
      l($dsid, 'islandora/object/' . $object->id . '/datastream/' . $dsid . '/version',
        array('attributes' => array('target' => '_blank'))),
    );
    if ($diff_exists) {
      try {
        $history = $object->repository->api->m->getDatastreamHistory($object->id, $dsid);
        $before_version = islandora_find_replace_get_version_id($history, $info['before']);
        $after_version = islandora_find_replace_get_version_id($history, $info['after']);
        if (!is_null($before_version) && !is_null($after_version)) {
          $success_rows[$pid][] = l(
            t('View Diff'),
            "islandora/object/{$object->id}/datastream/{$dsid}/diff/{$before_version}/{$after_version}",
            array('attributes' => array('target' => '_blank'))
          );
        }
        else {
          $success_rows[$pid][] = t('Not available');
        }
      }
      catch (Exception $e) {
        $success_rows[$pid][] = t('Not available');
      }
    }
  }

  $fail_output = '';
  if (isset($revisions['fail']) && count($revisions['fail'])) {
    $fail_output = t('<h2>Failed Replacements</h2>');
    $fail_header = array(
      t('Label'),
      t('Datastream Versions'),
    );
    $fail_rows = array();
    foreach ($revisions['fail'] as $fail_pid) {
      $object = islandora_object_load($fail_pid);
      $fail_rows[$fail_pid] = array(
        l($object->label, 'islandora/object/' . $object->id,
          array('attributes' => array('target' => '_blank'))),
        l($dsid, 'islandora/object/' . $object->id . '/datastream/' . $dsid . '/version',
          array('attributes' => array('target' => '_blank'))),
      );
    }
    $fail_output .= theme('table', array('header' => $fail_header, 'rows' => $fail_rows));
  }
  $output = array(
    'success' => array(
      '#type' => 'markup',
      '#markup' => t('<h2>Successful Replacements</h2>') . theme('table',
          array('header' => $success_header, 'rows' => $success_rows)),
    ),
  );
  $output['revert'] = drupal_get_form('revert_form');
  if (!empty($fail_output)) {
    $output['fail'] = array(
      '#type' => 'markup',
      '#markup' => $fail_output,
    );
  }

  return $output;
}

/**
 * Get the datastream's version ID based on a recorded datastream location.
 *
 * @param array $history
 *   A history of the datastream's modifications.
 * @param string $location
 *   The Fedora datastream location.
 *
 * @return int|null
 *   Return NULL if the version ID can't be found.
 */
function islandora_find_replace_get_version_id($history, $location) {
  foreach ($history as $version => $ds) {
    if ($ds['dsLocation'] == $location) {
      return $version;
    }
  }
  return NULL;
}

/**
 * gets the log id form uri
 *
 * @return string $id
 * Id of current page log
 */
function get_id_from_path() {
  $path = explode('/', current_path());
  $id = '';
  if (gettype((int)$path['5']) == 'integer') {
    $id = $path['5'];
  }
  return $id;
}

/**
 * get record of current log from database
 *
 * @param string $id
 * the id of the current log
 * @param string $field
 * field from database, either revisions or dsid
 */
function get_log_field($id, $field) {
  if($field != 'revisions' && $field != 'dsid') {
    throw new Exception("$field is not legal");
  }
  $query = db_query("SELECT $field FROM {islandora_find_replace} WHERE id=:logid", array(':logid' => $id));
  return $query;
}

/**
 * helper to unserialize revised pid log from database query
 *
 * @param object $query
 * database object of revisions from batch log
 *
 * @return array $changed
 * array of pids sucessfully changed by batch replace
 */
function get_changed_pids_or_revert_version($query, $revert = FALSE) {
  $result = $query->fetchField();
  $chunks = unserialize($result);
  if (isset($chunks)) {
    $changed_or_revert = $revert ? $chunks['success'] : array_keys($chunks['success']);
    return $changed_or_revert;
  }
}


/**
 * revert changes made by batch log.
 */
function revert_find_replace() {
  $id = get_id_from_path();
  $query = get_log_field($id,'revisions');
  $csv = get_changed_pids_or_revert_version($query);
  $dsid = get_log_field($id, 'dsid');
  $query_a = get_log_field($id,'revisions');
  $before_version = get_changed_pids_or_revert_version($query_a, TRUE);
  $dsid = $dsid->fetchField();
  $operations = array();
  foreach ($csv as $pid) {
    $operations[] = array(
      'islandora_revert_datastream',
      array(
        $pid, $dsid, $before_version[$pid]['before'],
      ),
    );
  }
  return $operations;
}

/**
 * implements hook_form.
 */
function revert_form($form, &$form_state) {
  $form['revert'] = array(
    '#type' => 'submit',
    '#value' => 'Revert this batch',
  );
  return $form;
}

/**
 * implements hook_form_submit.
 */
function revert_form_submit($form, &$form_state) {
  $batch = array(
    'operations' => revert_find_replace(),
    'title' => t('Revert each object'),
    'init_message' => t('Beginning revert.'),
    'finished' => 'revert_finished',
    'progress_message' => t('Processed @current out of @total.'),
  );
  batch_set($batch);
  $form_state['redirect'] = 'admin/islandora/tools/find-replace/find';
}
