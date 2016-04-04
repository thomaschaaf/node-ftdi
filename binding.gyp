{
  'targets': [
    {
      'target_name': 'ftdi',
      'sources':
      [
        'src/ftdi_device.cc',
	      'src/ftdi_driver.cc'
      ],
      'include_dirs+':
      [
        'src/',
         "<!(node -e \"require('nan')\")"
      ],
      'conditions':
      [
        ['OS == "win"',
          {
            'include_dirs+':
            [
              'lib/'
            ],
            'link_settings':
            {
              "conditions" :
              [
                ["target_arch=='ia32'",
                {
                  'libraries':
                  [
                   '-l<(module_root_dir)/lib/i386/ftd2xx.lib'
                  ]
                }
              ],
              ["target_arch=='x64'", {
                'libraries': [
                   '-l<(module_root_dir)/lib/amd64/ftd2xx.lib'
                ]
              }]
            ]
          }
        }],
        ['OS != "win"',
          {
            'include_dirs+': [
              '/usr/local/include',
              '/usr/local/include/libftd2xx/'
            ],
            'ldflags': [
              '-Wl,-Map=output.map',
            ],
            'link_settings': {
              'libraries': [
                '-lftd2xx'
              ]
            }
          }
        ]
      ],
    }
  ]
}
