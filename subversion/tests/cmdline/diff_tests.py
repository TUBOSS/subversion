from svntest.verify import make_diff_header, make_no_diff_deleted_header, \
                           make_diff_header, make_no_diff_deleted_header, \
                           make_git_diff_header, make_diff_prop_header, \
                           make_diff_prop_val, make_diff_prop_deleted, \
                           make_diff_prop_added, make_diff_prop_modified
  svntest.actions.run_and_verify_svn(None, None,
                                     'svn: E155010: .*foo\' was not found.',
                                     'diff', sbox.ospath('A/D/foo'))
                                                "nonexistent") + [
                                                "nonexistent") + [
  expected_output_base_r2 = make_diff_header("foo", "nonexistent",
  expected_output_r1_base = make_diff_header("foo", "nonexistent",
                                                "nonexistent") + [
  expected_output_base_working[3] = "+++ foo\t(nonexistent)\n"
  diff_X_r1_base = make_diff_header("X", "nonexistent",
  diff_X_base_r3 = make_diff_header("X", "nonexistent",
  diff_foo_r1_base = make_diff_header("foo", "nonexistent",
  diff_foo_base_r3 = make_diff_header("foo", "nonexistent",
  diff_X_bar_r1_base = make_diff_header("X/bar", "nonexistent",
  diff_X_bar_base_r3 = make_diff_header("X/bar", "nonexistent",
  expected_output_r1_BASE = make_diff_header("X/bar", "nonexistent",
  expected_output_r1_WORKING = make_diff_header("X/bar", "nonexistent",
  diff_repos_wc = make_diff_header("A/mucopy", "revision 2", "nonexistent")
                         "nonexistent", "working copy",
                                         "nonexistent",
  ] + make_git_diff_header(new_path, "new", "nonexistent",
                                         "revision 1", "nonexistent",
                           "revision 1", "nonexistent",
                           "revision 1", "nonexistent",
  expected_output = make_git_diff_header(new_path, "new", "nonexistent",
  ] + make_git_diff_header(mu_path, "A/mu", "revision 1", "nonexistent",
                                         "nonexistent",
    ] + make_git_diff_header("new", "new", "nonexistent", "revision 2",
  expected_output = make_git_diff_header(new_path, "new", "nonexistent",
                                         "nonexistent", "working copy",
  expected_output = make_diff_header('newdir/newfile', 'nonexistent',
                                         'nonexistent',
                    ] + make_diff_header('A/B/F', 'nonexistent',
                        make_diff_prop_added("newprop",
                                         'nonexistent',
                    ] + make_diff_header('A/D/G/pi', 'nonexistent',
                                         'nonexistent',
                                         'nonexistent',
                                         'nonexistent',
                    ] + make_diff_header('A/B/F', 'working copy',
                                         'nonexistent',
                                         src_label, dst_label) + \
                        make_diff_prop_header('A/B/F') + \
                        make_diff_prop_deleted('newprop', 'propval-old\n')

  # Files in diff may be in any order. #### Not any more, but test order is wrong.
  expected_output = make_diff_header("chi", "revision 1", "nonexistent") + [
                                         "nonexistent") + [
                                         "nonexistent") + [
  expected_output = make_diff_header("G/pi", "nonexistent", "working copy",
                    ] + make_diff_header("G/rho", "nonexistent",
                    ] + make_diff_header("G/tau", "nonexistent",
                    ] + make_diff_header("H/chi", "nonexistent",
                    ] + make_diff_header("H/omega", "nonexistent",
                    ] + make_diff_header("H/psi", "nonexistent",
                                         "nonexistent", "B/E", "D") + [
                                         "nonexistent", "B/E", "D") + [
                    ] + make_diff_header("gamma", "nonexistent",
    '+++ %s\t(nonexistent)\n' % sbox.path('A/B/E/alpha'),
    '+++ %s\t(nonexistent)\n' % sbox.path('A/B/E/beta'),
    '--- %s\t(nonexistent)\n' % sbox.path('A/B/E'),
    '+++ %s\t(nonexistent)\n' % sbox.path('A/B/E/alpha'),
    '+++ %s\t(nonexistent)\n' % sbox.path('A/B/E/beta'),
    '--- %s\t(nonexistent)\n' % sbox.path('A/B/E/beta'),
    '--- %s\t(nonexistent)\n' % sbox.path('A/B/E'),
    '+++ %s\t(nonexistent)\n' % sbox.path('A/B/E/alpha'),
  expected_output = make_diff_header(iota_copy, "nonexistent", "working copy",
    expected_output = make_diff_header(newfile, 'nonexistent', 'revision 2') + \
    expected_output = make_diff_header(newfile, 'revision 2', 'nonexistent') + \
                      [ '@@ -1 +0,0 @@\n',
                        "-This is the file 'newfile'.\n",
                        '\n',
                        'Property changes on: %s\n' % sbox.path('newfile'),
                        '__________________________________________________' +
                              '_________________\n',
                        'Deleted: svn:mime-type\n',
                        '## -1 +0,0 ##\n',
                        '-text/plain\n',
                        '\ No newline at end of property\n']
    svntest.actions.run_and_verify_svn(None, expected_output, [], 'diff',
def diff_switched_file(sbox):
  "diff a switched file against repository"

  sbox.build()
  svntest.actions.run_and_verify_svn(None, None, [], 'switch',
                                     sbox.repo_url + '/A/mu',
                                     sbox.ospath('iota'), '--ignore-ancestry')
  sbox.simple_append('iota', 'Mu????')

  # This diffs the file against its origin
  expected_output = [
    'Index: %s\n' % sbox.path('iota'),
    '===================================================================\n',
    '--- %s\t(.../A/mu)\t(revision 1)\n' % sbox.path('iota'),
    '+++ %s\t(.../iota)\t(working copy)\n' % sbox.path('iota'),
    '@@ -1 +1,2 @@\n',
    ' This is the file \'mu\'.\n',
    '+Mu????\n',
    '\ No newline at end of file\n',
  ]
  svntest.actions.run_and_verify_svn(None, expected_output, [],
                                     'diff', '-r', '1', sbox.ospath('iota'))

  # And this undoes the switch for the diff
  expected_output = [
    'Index: %s\n' % sbox.path('iota'),
    '===================================================================\n',
    '--- %s\t(revision 1)\n' % sbox.path('iota'),
    '+++ %s\t(working copy)\n' % sbox.path('iota'),
    '@@ -1 +1,2 @@\n',
    '-This is the file \'iota\'.\n',
    '+This is the file \'mu\'.\n',
    '+Mu????\n',
    '\ No newline at end of file\n',
  ]
  svntest.actions.run_and_verify_svn(None, expected_output, [],
                                     'diff', '-r', '1', sbox.ospath(''))


              diff_switched_file,