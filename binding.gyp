{
  'targets': [
    {
      'target_name': 'ftdi',
      'sources': 
      [
        'src/node_ftdi.cc',
	      'src/ftdi_driver.cc'
      ],
      'include_dirs+': 
      [
        'src/',
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
                   '-l<(module_root_dir)/lib/i386/ftd2xx'
                  ]
                }
              ],
              ["target_arch=='x64'", {
                'libraries': [
                   '-l<(module_root_dir)/lib/amd64/ftd2xx'
                ]
              }]
            ]
          }
        }],
        ['OS != "win"',
          {
            'include_dirs+': [
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
