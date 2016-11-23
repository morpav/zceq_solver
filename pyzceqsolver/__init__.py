import logging

log = logging.getLogger('{0}'.format(__name__))

def get_library_filename(system_name):
    library_name_map_fmt = {
        'Linux': 'lib{}.so',
        'Windows': '{}.dll',
    }
    try:
        library_filename = library_name_map_fmt[system_name].format(
            'zceqsolver')
    except KeyError as e:
        msg = 'Unsupported system: {0}, cannot provide system specific ' \
              'library name'.format(system_name)
        raise Exception(msg)
    return library_filename
