#include <stdio.h>
#include <time.h>
#include <openssl/ocsp.h>
#include <openssl/x509.h>
#include <openssl/err.h>
int main(void) {
    time_t now = time(NULL);
    printf("now=%lld ctime=%s", (long long)now, ctime(&now));
    ASN1_TIME *tu = ASN1_TIME_set(NULL, now - 3600);
    ASN1_TIME *nu = ASN1_TIME_set(NULL, now + 7L*86400);
    int c1 = X509_cmp_time(tu, &now);
    int c2 = X509_cmp_time(nu, &now);
    printf("cmp(thisUpd,now)=%d (expect <0)\n", c1);
    printf("cmp(nextUpd,now)=%d (expect >0)\n", c2);
    char buf[256];
    unsigned long e;
    while ((e = ERR_get_error())) { ERR_error_string_n(e, buf, sizeof buf); printf("err: %s\n", buf); }
    int ok = OCSP_check_validity(tu, nu, 0, 0);
    printf("validity(0,0)=%d\n", ok);
    while ((e = ERR_get_error())) { ERR_error_string_n(e, buf, sizeof buf); printf("err: %s\n", buf); }
    return 0;
}
