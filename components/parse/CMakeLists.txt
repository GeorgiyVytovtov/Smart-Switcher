idf_component_register(
    SRCS "parse.c"
    REQUIRES 
        json
        shearch_components
    INCLUDE_DIRS "."
)