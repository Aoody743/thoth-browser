add_cxx_compile_options(-Wvla)

set(SOURCES
    TLSv12.cpp
)

ladybird_lib(LibTLS tls)
target_link_libraries(LibTLS PRIVATE LibCore LibCrypto LibFileSystem)

target_link_libraries(LibTLS PUBLIC OpenSSL::SSL)

if (WIN32)
    # CertOpenSystemStoreW / CertEnumCertificatesInStore for the Windows root store.
    target_link_libraries(LibTLS PRIVATE crypt32)
endif()
