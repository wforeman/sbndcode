include_directories ( $ENV{GENIE_INC}/GENIE )

art_make(
            LIB_LIBRARIES
            larcore_Geometry_Geometry_service
            larcorealg_Geometry
            nugen_NuReweight_art
            nugen_NuReweight
            nusimdata_SimulationBase
            ${ART_FRAMEWORK_CORE}
            ${ART_FRAMEWORK_PRINCIPAL}
            ${ART_FRAMEWORK_SERVICES_REGISTRY}
            ${ART_ROOT_IO_TFILE_SUPPORT} ${ROOT_CORE}
            art_Persistency_Common
            art_Persistency_Provenance
            art_Utilities
            canvas
            ${MF_MESSAGELOGGER}

            ${FHICLCPP}
            cetlib cetlib_except
            ${CLHEP}
            ${Boost_SYSTEM_LIBRARY}
            ${ROOT_BASIC_LIB_LIST}
          )

install_headers()

