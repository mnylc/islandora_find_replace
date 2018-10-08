<?php
/**
 * @file
 *   Drush Command for getting pids from individual batch.
 */

/**
 * Implements hook_drush_command().
 */
function islandora_find_replace_drush_command() {
  $items = array();
  $items['islandora-find-replace-csv-from-log'] = array(
    'description' => dt('Saves a csv file of all pids in a given batch'),
    'examples' => array(
      dt('drush islandora-find-replace-csv-from-log -u 1 --path=/tmp/csv.tmp --log_id=1'),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'drupal dependencies' => array(
      'islandora',
    ),
    'options' => array(
      'path' => array(
        'description' => dt('path to save file to'),
        'required' => TRUE,
      ),
      'log_id' => array(
        'description' => dt('ID of the log you wish to retrieve pids from. listed at islandora/tools/find-replace/log'),
        'required' => TRUE,
      )
    )
  );
  return $items;
}

/**
 * get revised pids from database
 *
 * @param string $id
 * the id of the current log
 */
function get_revision_pids($id) {
  $query = db_query("SELECT revisions FROM {islandora_find_replace} WHERE id=:logid", array(':logid' => $id));
  return $query;
}

/**
 * Unserialize revised pid log from database query.
 *
 * @param object $query
 * database object of revisions from batch log
 *
 * @return array $changed
 * array of pids sucessfully changed by batch replace
 */
function get_changed_pids($query, $revert = FALSE) {
  $result = $query->fetchField();
  $dict = unserialize($result);
  if (isset($dict)) {
    $changed_or_revert = $revert ? $dict['success'] : array_keys($dict['success']);
    return $changed_or_revert;
  }
}

/**
 * prepare sucessfully revised objects pids as a csv
 */
function drush_islandora_find_replace_csv_from_log() {
  $path = drush_get_option('path');
  $log_id = drush_get_option('log_id');
  $query = get_revision_pids($log_id);
  $csv = implode(',', get_changed_pids($query));
  file_put_contents($path, $csv);
}
