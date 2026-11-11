// Run the actual source hooks under ARM CPU translation, without Android OS
// initialization. A pass proves these native function bodies can return and
// preserve their side effects; it does not validate a phone or game runtime.
static_assert(sizeof(void *) == 4);
extern "C" {
char data_0209e660[1] = {1};
void _ZN3OAM12EnableSubOAMEv();
void _ZN8dActor_c11OnAttacked1ERS_(void *, void *);
void _ZN8dActor_c11OnAttacked2ERS_(void *, void *);
void _ZN8dActor_c19OnHitFromUnderneathERS_(void *, void *);
void _ZN8dActor_c24OnHitByCannonBlastedCharERS_(void *, void *);
void _ZN8dActor_c8OnPushedERS_(void *, void *);
}
int main() {
    int untouched = 73;
    _ZN8dActor_c11OnAttacked1ERS_(&untouched, &untouched);
    _ZN8dActor_c11OnAttacked2ERS_(&untouched, &untouched);
    _ZN8dActor_c19OnHitFromUnderneathERS_(&untouched, &untouched);
    _ZN8dActor_c24OnHitByCannonBlastedCharERS_(&untouched, &untouched);
    _ZN8dActor_c8OnPushedERS_(&untouched, &untouched);
    _ZN3OAM12EnableSubOAMEv();
    if (untouched != 73 || data_0209e660[0] != 0) __builtin_trap();
    return 0;
}
