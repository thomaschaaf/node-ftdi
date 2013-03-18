{
  'targets': [
    {
      'target_name': 'ftdi',
      'sources': [
        'src/node_ftdi.cc',
	      'src/ftdi_driver.cc'
      ],
      'include_dirs+': [
        'src/',
      ],
      'conditions': [
        ['OS == "win"',
          {
            'include_dirs+': [
              'lib/'
            ]
          }
        ],
        ['OS != "win"',
          {
              'include_dirs+': [
                '/usr/local/include/libftd2xx/'
              ]
          }
        ]
      ],
      'link_settings': {
        'conditions' : [
          ['OS != "win"',
            {
              'libraries': [
                '-lftd2xx'
              ]
            }
          ],
          ['OS == "win"',
            {
              'libraries': [
                '-l<(module_root_dir)/lib/amd64/ftd2xx'
              ]
            }
          ]
        ]
      }      
    }
  ]
}
