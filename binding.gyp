{
  'targets': [
    {
      'target_name': 'ftdi',
      'sources': [
        'src/node_ftdi.cc',
	      'src/node_ftdi.h',
	      'src/ftdi_driver.cc'
      ],
      'include_dirs+': [
        'src/',
	      '/usr/local/include/libftd2xx/'
      ],
      'link_settings': {
        'conditions' : [
            ['OS != "win"',
                {
                    'libraries': [
                      '-lftd2xx'
                    ]
                }
            ]
        ]
      }      
    }
  ]
}
