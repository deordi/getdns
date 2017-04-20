YAML to JSON conversion

TODO

1) libyaml is a dependency. Change so that yaml support is optional.
   Requires changes to configure.ac, /src/tools/Makefile.in and getdns_query.c (and convert_yaml_to_json?)
   
2) Review where stubby gets configuration data from. At present stubby reads from 
        the default configuration string and
        /etc/stubby.conf and
        ~/.stubby.conf and
        the file named in the -C option
    Is this the behaviour you want?

3) Testing.
 a. The code has only been tested on yaml data using indentation style.
    It should be tested on other styles as well.
 b. Restrictions on the yaml supported:
      the outer-most data structure must be a yaml mapping
      mapping keys must be yaml scalars
      plain scalars are output to the json string unchanged
      non-plain scalars (quoted, double-quoted, wrapped) are output double-quoted
    yaml tags can be used to specify data type. At present the code ignores tags.
    Test on yaml data containing yaml tags (these are ignored at present)
 c. Test on very large config file.
 d. Unit or tpackage tests.

4) The gbuffer used to hold the json output is currently fixed at 8192 bytes. 
   If the buffer fills up before data conversion has finished the data in the .yaml file will be ignored. 
   Change to allow the buffer to grow.

5) Consider how to recognise yaml configuration files. At present .yaml ending is used.
