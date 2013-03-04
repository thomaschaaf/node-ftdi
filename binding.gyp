{
  'targets': [
    {
      'target_name': 'node-ftdi',
      'sources': [
        'src/node_ftdi.cc'
      ],
      'include_dirs+': [
        'src/'
      ],
      'link_settings': {
        'conditions' : [
            ['OS != "win"',
                {
                    'libraries': [
                      '-lftdi'
                    ]
                }
            ]
        ]
      }      
    }
  ]
}
