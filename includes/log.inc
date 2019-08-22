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
    t('PID'),
    t('Label'),
    t('Datastream Versions'),
  );
  if ($diff_exists) {
    $success_header[] = t('Datastream Diff');
  }

  $success_rows = array();
  foreach ($revisions['success'] as $pid => $info) {
    $object = islandora_object_load($pid);
    if ($object) {
      $success_rows[$pid] = array(
        t($pid),
        l($object->label, 'islandora/object/' . $object->id,
          array('attributes' => array('target' => '_blank'))),
        l($dsid, 'islandora/object/' . $object->id . '/datastream/' . $dsid . '/version',
          array('attributes' => array('target' => '_blank'))),
      );
    }
    else {
      $orig = $info['before'];
      $after = $info['after'];
      $success_rows[$pid] = array(
        "Object $pid Deleted." ,
        "Datastream $dsid not available. Before version: $orig to after version: $after, files not available."
      );
    }
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
      t('PID'),
      t('Label'),
      t('Datastream Versions'),
    );
    $fail_rows = array();
    foreach ($revisions['fail'] as $fail_pid) {
      $object = islandora_object_load($fail_pid);
      $fail_rows[$fail_pid] = array(
        t($pid),
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
